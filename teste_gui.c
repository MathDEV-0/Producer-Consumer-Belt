/**
 * NOTA ACADÊMICA:
 * 
 * Este módulo de visualização gráfica foi desenvolvido como ferramenta complementar
 * para fins de observação e depuração do sistema produtor-consumidor.
 * 
 * A implementação da interface gráfica utilizando OpenGL/GLUT foi auxiliada
 * por ferramentas de IA, visando exclusivamente a melhoria da experiência
 * de visualização e análise do comportamento do sistema.
 * 
 * O núcleo do sistema (sincronização com mutex e variáveis de condição,
 * lógica de produção/consumo, gerenciamento do buffer circular) é de
 * autoria própria e constitui a implementação principal do trabalho.
 * 
 * A camada gráfica não interfere na lógica de negócio do sistema,
 * sendo utilizada apenas para fins demonstrativos e de observabilidade.
 */

//Para rodar com gráficos:
//gcc -pthread teste_gui.c -o prod_cons_graphics -DUSE_GRAPHICS -lfreeglut -lglu32 -lopengl32
//./prod_cons_graphics

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <math.h>

#define GOOD_QUALITY 0
#define BAD_QUALITY 1
#define N_PROD 3
#define N_CONS 2
#define BUFFER_LEN 3

typedef struct{
    short int id;
    char quality;
    short int type;
    struct timespec time_create;
    struct timespec time_out;
}Product;

Product buffer[BUFFER_LEN];
int count = 0, global_id = 1;
int total_produced = 0, total_consumed = 0, total_discarded = 0;
int total_B = 0, total_M = 0;
double time_on_belt = 0.0;
int running = 1;

// Para animação dos produtores/consumidores
int producer_activity[N_PROD] = {0};  // 0 = idle, 1 = producing
int consumer_activity[N_CONS] = {0};  // 0 = idle, 1 = consuming
time_t last_producer_time[N_PROD];
time_t last_consumer_time[N_CONS];

void* p_func(void *args);
void* c_func(void *args);
Product createProduct();
int insertProduct(Product p);
Product removeProduct();
void handle_sigint(int sig);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

#ifdef USE_GRAPHICS
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>

int window_width = 1024;
int window_height = 768;
int animation_frame = 0;

// Função para desenhar retângulo
void drawRect(float x, float y, float w, float h, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
    glEnd();
}

// Função para desenhar círculo (aproximado)
void drawCircle(float cx, float cy, float r, float R, float G, float B) {
    glColor3f(R, G, B);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for(int i = 0; i <= 360; i += 10) {
            float angle = i * 3.14159f / 180.0f;
            float x = cx + r * cos(angle);
            float y = cy + r * sin(angle);
            glVertex2f(x, y);
        }
    glEnd();
}

// Função para desenhar texto
void drawStr(float x, float y, const char* text) {
    glColor3f(1, 1, 1);
    glRasterPos2f(x, y);
    for(const char* c = text; *c; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
}

void drawStrBig(float x, float y, const char* text) {
    glColor3f(1, 1, 1);
    glRasterPos2f(x, y);
    for(const char* c = text; *c; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
}

// Desenha produtores
void drawProducers() {
    float start_x = 50;
    float start_y = window_height - 250;
    float box_w = 120;
    float box_h = 100;
    
    drawStr(start_x, start_y + 20, "=== PRODUCERS ===");
    
    for(int i = 0; i < N_PROD; i++) {
        float x = start_x + i * (box_w + 20);
        float y = start_y - box_h;
        
        // Fundo do produtor
        if(producer_activity[i] == 1) {
            drawRect(x, y, box_w, box_h, 1.0f, 0.8f, 0.0f);  // Amarelo quando produzindo
        } else {
            drawRect(x, y, box_w, box_h, 0.2f, 0.2f, 0.4f);  // Azul escuro quando idle
        }
        
        // Borda
        glColor3f(0.8f, 0.8f, 0.8f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(x, y);
            glVertex2f(x + box_w, y);
            glVertex2f(x + box_w, y + box_h);
            glVertex2f(x, y + box_h);
        glEnd();
        
        // Texto do produtor
        char txt[64];
        sprintf(txt, "PRODUCER %d", i);
        drawStr(x + 10, y + box_h - 20, txt);
        
        if(producer_activity[i] == 1) {
            drawStr(x + 10, y + box_h - 45, "⚙️ PRODUCING...");
            
            // Animação de engrenagem
            float gear_x = x + box_w - 25;
            float gear_y = y + 25;
            drawCircle(gear_x, gear_y, 12, 1.0f, 0.5f, 0.0f);
            drawCircle(gear_x, gear_y, 6, 0.0f, 0.0f, 0.0f);
        } else {
            drawStr(x + 10, y + box_h - 45, "💤 IDLE");
        }
        
        sprintf(txt, "Prod: %d", total_produced / (i+1));  // Produtos por produtor
        drawStr(x + 10, y + 10, txt);
    }
}

// Desenha consumidores
void drawConsumers() {
    float start_x = window_width - (N_CONS * 140) - 50;
    float start_y = window_height - 250;
    float box_w = 120;
    float box_h = 100;
    
    drawStr(start_x, start_y + 20, "=== CONSUMERS ===");
    
    for(int i = 0; i < N_CONS; i++) {
        float x = start_x + i * (box_w + 20);
        float y = start_y - box_h;
        
        // Fundo do consumidor
        if(consumer_activity[i] == 1) {
            drawRect(x, y, box_w, box_h, 0.0f, 0.8f, 0.8f);  // Ciano quando consumindo
        } else {
            drawRect(x, y, box_w, box_h, 0.4f, 0.2f, 0.2f);  // Vermelho escuro quando idle
        }
        
        // Borda
        glColor3f(0.8f, 0.8f, 0.8f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(x, y);
            glVertex2f(x + box_w, y);
            glVertex2f(x + box_w, y + box_h);
            glVertex2f(x, y + box_h);
        glEnd();
        
        // Texto do consumidor
        char txt[64];
        sprintf(txt, "CONSUMER %d", i);
        drawStr(x + 10, y + box_h - 20, txt);
        
        if(consumer_activity[i] == 1) {
            drawStr(x + 10, y + box_h - 45, "🍽️ CONSUMING...");
            
            // Animação de boca
            float mouth_x = x + box_w - 25;
            float mouth_y = y + 25;
            drawCircle(mouth_x, mouth_y, 10, 0.8f, 0.2f, 0.2f);
        } else {
            drawStr(x + 10, y + box_h - 45, "😴 SLEEPING");
        }
        
        sprintf(txt, "Cons: %d", total_consumed / (i+1));
        drawStr(x + 10, y + 10, txt);
    }
}

// Função de desenho principal
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Configura viewport 2D
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, window_width, 0, window_height);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Título
    drawStrBig(window_width/2 - 150, window_height - 30, "PRODUCER-CONSUMER SIMULATOR");
    
    // Desenha produtores e consumidores
    drawProducers();
    drawConsumers();
    
    // Desenha buffer
    float slot_w = (window_width - 200.0f) / BUFFER_LEN;
    float slot_h = 100.0f;
    float start_x = (window_width - (BUFFER_LEN * slot_w)) / 2;
    float y = window_height - 450;
    
    drawStr(start_x, y + 30, "═══════════════ BUFFER CIRCULAR ═══════════════");
    
    for(int i = 0; i < BUFFER_LEN; i++) {
        float x = start_x + i * slot_w;
        
        // Desenha fundo do slot
        if(buffer[i].id != 0) {
            if(buffer[i].quality == GOOD_QUALITY)
                drawRect(x, y, slot_w - 4, slot_h, 0.2f, 0.7f, 0.2f);  // Verde
            else
                drawRect(x, y, slot_w - 4, slot_h, 0.7f, 0.2f, 0.2f);  // Vermelho
        } else {
            drawRect(x, y, slot_w - 4, slot_h, 0.3f, 0.3f, 0.3f);  // Cinza
        }
        
        // Borda
        glColor3f(0.8f, 0.8f, 0.8f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(x, y);
            glVertex2f(x + slot_w - 4, y);
            glVertex2f(x + slot_w - 4, y + slot_h);
            glVertex2f(x, y + slot_h);
        glEnd();
        
        // Texto do slot
        char txt[32];
        if(buffer[i].id != 0) {
            sprintf(txt, "ID: %d", buffer[i].id);
            drawStr(x + 5, y + slot_h - 25, txt);
            sprintf(txt, "%s", buffer[i].quality == GOOD_QUALITY ? "✅ BOM" : "❌ RUIM");
            drawStr(x + 5, y + slot_h - 45, txt);
        } else {
            drawStr(x + 5, y + slot_h - 35, "⚪ VAZIO");
        }
        
        sprintf(txt, "[%d]", i);
        drawStr(x + 5, y + 5, txt);
    }
    
    // Setas indicando fluxo
    float mid_y = window_height - 450 + slot_h/2;
    drawStr(start_x - 60, mid_y, "➡️");
    drawStr(start_x + (BUFFER_LEN * slot_w) + 20, mid_y, "⬅️");
    
    // Indicador animado (mostra próximo slot)
    float anim_x = start_x + (animation_frame % BUFFER_LEN) * slot_w;
    drawRect(anim_x, y - 10, slot_w - 4, 5, 1.0f, 1.0f, 0.0f);
    
    // Estatísticas (parte inferior)
    float stats_y = 80;
    char stats[256];
    
    drawStr(50, stats_y + 160, "═══════════════ STATISTICS ═══════════════");
    
    sprintf(stats, "📊 TOTAL PRODUCED: %d", total_produced);
    drawStr(50, stats_y + 130, stats);
    sprintf(stats, "📊 TOTAL CONSUMED: %d", total_consumed);
    drawStr(50, stats_y + 105, stats);
    sprintf(stats, "📊 TOTAL DISCARDED: %d", total_discarded);
    drawStr(50, stats_y + 80, stats);
    sprintf(stats, "📦 BUFFER: %d / %d", count, BUFFER_LEN);
    drawStr(50, stats_y + 55, stats);
    
    // Qualidade
    sprintf(stats, "✅ GOOD (B): %d", total_B);
    drawStr(window_width/2, stats_y + 130, stats);
    sprintf(stats, "❌ BAD (M): %d", total_M);
    drawStr(window_width/2, stats_y + 105, stats);
    float ratio = total_produced ? (float)total_B/total_produced*100 : 0;
    sprintf(stats, "📈 GOOD RATIO: %.1f%%", ratio);
    drawStr(window_width/2, stats_y + 80, stats);
    
    double avg = total_consumed ? time_on_belt/total_consumed : 0;
    sprintf(stats, "⏱️ AVG TIME ON BELT: %.3f s", avg);
    drawStr(window_width/2, stats_y + 55, stats);
    
    // Barra de progresso do buffer
    float bar_w = 300;
    float bar_h = 20;
    float fill = (count / (float)BUFFER_LEN) * bar_w;
    drawRect(50, stats_y - 10, bar_w, bar_h, 0.3f, 0.3f, 0.3f);
    drawRect(50, stats_y - 10, fill, bar_h, 0.0f, 0.8f, 0.0f);
    sprintf(stats, "BUFFER UTILIZATION: %.0f%%", (count/(float)BUFFER_LEN)*100);
    drawStr(50 + bar_w + 10, stats_y - 5, stats);
    
    // Taxas
    sprintf(stats, "📈 PROD RATE: %.1f p/s", total_produced / (time(NULL) - 1.0));
    drawStr(50, stats_y - 40, stats);
    sprintf(stats, "📉 CONS RATE: %.1f c/s", total_consumed / (time(NULL) - 1.0));
    drawStr(300, stats_y - 40, stats);
    
    glutSwapBuffers();
    
    animation_frame++;
    if(animation_frame >= BUFFER_LEN * 5) animation_frame = 0;
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
}

void timer(int value) {
    glutPostRedisplay();
    glutTimerFunc(33, timer, 0);  // ~30 FPS para animação suave
}

void keyboard(unsigned char key, int x, int y) {
    if(key == 27) {  // ESC
        running = 0;
        exit(0);
    }
}

void initGL() {
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// Função principal do OpenGL (roda na thread principal)
void runGraphics(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Producer-Consumer Simulator - Visual Edition");
    
    initGL();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(33, timer, 0);
    
    glutMainLoop();
}
#endif

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_sigint);
    srand(time(NULL));
    
    pthread_t producer[N_PROD];
    pthread_t consumer[N_CONS];
    
#ifdef USE_GRAPHICS
    // Executa OpenGL na thread principal
    for(int i = 0; i < N_PROD; i++) {
        pthread_create(&producer[i], NULL, p_func, (void*)(intptr_t)i);
    }
    
    for(int i = 0; i < N_CONS; i++) {
        pthread_create(&consumer[i], NULL, c_func, (void*)(intptr_t)i);
    }
    
    // Roda OpenGL na thread principal (bloqueante)
    runGraphics(argc, argv);
    
    // Só chega aqui quando a janela for fechada
    for(int i = 0; i < N_PROD; i++) {
        pthread_join(producer[i], NULL);
    }
    
    for(int i = 0; i < N_CONS; i++) {
        pthread_join(consumer[i], NULL);
    }
#else
    // Versão sem gráficos
    for(int i = 0; i < N_PROD; i++) {
        pthread_create(&producer[i], NULL, p_func, (void*)(intptr_t)i);
    }
    
    for(int i = 0; i < N_CONS; i++) {
        pthread_create(&consumer[i], NULL, c_func, (void*)(intptr_t)i);
    }
    
    while(running) {
        sleep(2);
        printf("\n╔════════════════════════════════╗\n");
        printf("║         STATISTICS              ║\n");
        printf("╠════════════════════════════════╣\n");
        printf("║ Produced: %-15d ║\n", total_produced);
        printf("║ Consumed: %-15d ║\n", total_consumed);
        printf("║ Buffer: %-15d/%d ║\n", count, BUFFER_LEN);
        printf("║ Good: %-17d ║\n", total_B);
        printf("║ Bad: %-18d ║\n", total_M);
        printf("║ Avg time: %-13.3fs ║\n", total_consumed ? time_on_belt/total_consumed : 0);
        printf("╚════════════════════════════════╝\n");
    }
    
    for(int i = 0; i < N_PROD; i++) {
        pthread_join(producer[i], NULL);
    }
    
    for(int i = 0; i < N_CONS; i++) {
        pthread_join(consumer[i], NULL);
    }
#endif
    
    return 0;
}

Product createProduct() {
    Product p;
    
    pthread_mutex_lock(&mutex);
    p.id = global_id++;
    pthread_mutex_unlock(&mutex);
    
    p.quality = (rand() % 2 == 0) ? GOOD_QUALITY : BAD_QUALITY;
    p.type = rand() % 3;
    clock_gettime(CLOCK_REALTIME, &p.time_create);
    return p;
}

int insertProduct(Product p) {
    for(int i = 0; i < BUFFER_LEN; i++) {
        if(buffer[i].id == 0) {
            buffer[i] = p;
            count++;
            return 1;
        }
    }
    total_discarded++;
    return 0;
}

Product removeProduct() {
    for(int i = 0; i < BUFFER_LEN; i++) {
        if(buffer[i].id != 0) {
            Product p = buffer[i];
            buffer[i].id = 0;
            count--;
            return p;
        }
    }
    return (Product){0, 0, 0, {0, 0}, {0, 0}};
}

void* p_func(void *args) {
    int id = (int)(intptr_t)args;
    while(running) {
        producer_activity[id] = 1;
        
        Product p = createProduct();
        
        pthread_mutex_lock(&mutex);
        while(count == BUFFER_LEN) {
            pthread_cond_wait(&not_full, &mutex);
        }
        
        total_produced++;
        int res = insertProduct(p);
        
        if(res == 1) {
            printf("[P%d] 🏭 Produced %d (%s) type %d\n", 
                   id, p.id, p.quality == GOOD_QUALITY ? "B" : "M", p.type);
        } else {
            printf("[P%d] ❌ Product %d discarded (buffer full)\n", id, p.id);
        }
        
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mutex);
        
        producer_activity[id] = 0;
        sleep(rand() % 2 + 2);
    }
    return NULL;
}

void* c_func(void *args) {
    int id = (int)(intptr_t)args;
    while(running) {
        pthread_mutex_lock(&mutex);
        while(count == 0) {
            pthread_cond_wait(&not_empty, &mutex);
        }
        
        Product p = removeProduct();
        total_consumed++;
        
        clock_gettime(CLOCK_REALTIME, &p.time_out);
        double diff = (p.time_out.tv_sec - p.time_create.tv_sec) +
                     (p.time_out.tv_nsec - p.time_create.tv_nsec) / 1e9;
        time_on_belt += diff;
        
        if(p.quality == GOOD_QUALITY) {
            total_B++;
        } else {
            total_M++;
        }
        
        consumer_activity[id] = 1;
        
        printf("[C%d] 🍽️ Consumed %d (%s) type %d (time on belt: %.3fs)\n", 
               id, p.id, p.quality == GOOD_QUALITY ? "B" : "M", p.type, diff);
        
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);
        
        consumer_activity[id] = 0;
        sleep(rand() % 2 + 1);
    }
    return NULL;
}

void handle_sigint(int sig) {
    running = 0;
    printf("\n\n╔══════════════════════════════════════╗\n");
    printf("║        FINAL STATISTICS              ║\n");
    printf("╠══════════════════════════════════════╣\n");
    printf("║ Total produced: %-20d ║\n", total_produced);
    printf("║ Total consumed: %-20d ║\n", total_consumed);
    printf("║ Total discarded: %-19d ║\n", total_discarded);
    printf("║ Good quality (B): %-19d ║\n", total_B);
    printf("║ Bad quality (M): %-20d ║\n", total_M);
    printf("║ Avg time on belt: %-17.6f s ║\n", total_consumed ? time_on_belt/total_consumed : 0);
    printf("╚══════════════════════════════════════╝\n");
    exit(0);
}