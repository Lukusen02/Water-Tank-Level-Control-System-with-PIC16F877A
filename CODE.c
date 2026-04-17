/*
 * =============================================================================
 * SISTEMA DE CONTROL DE NIVEL DE TANQUE - v4.0
 * =============================================================================
 * Compilador : CCS PCWH
 * PIC        : PIC16F877A @ 4 MHz (XT)
 *
 * CAMBIOS v4 respecto a v3:
 *   - Velocidades del motor rediseñadas para motores NEMA reales con A4988.
 *     Asumir FULL-STEP (MS1=MS2=MS3=GND). Si el motor usa microstepping,
 *     dividir los umbrales por el factor de microstepping (ej: /16).
 *   - TEST_MOTOR eliminado del codigo final. Si necesitas probar RC0
 *     visualmente en Proteus, pon UMBRAL_VEL1 = 500 temporalmente.
 * =============================================================================
 * MAPA DE PINES
 * =============================================================================
 *  ENTRADAS:
 *    RA0 = Sensor nivel 1
 *    RA1 = Sensor nivel 2
 *    RA2 = Sensor nivel 3
 *    RA3 = Sensor nivel 4
 *    RA5 = Sensor nivel 5
 *    RC3 = Pulsador velocidad (activo en bajo)
 *
 *  SALIDAS:
 *    RB0-RB3 = BCD para 74LS47
 *    RB4     = LED velocidad 1
 *    RB5     = LED velocidad 2
 *    RB6     = LED velocidad 3
 *    RC0     = STEP motor paso a paso
 *    RC1     = DIR  motor (fijo en 0)
 *    RC2     = Bomba (via transistor NPN)
 * =============================================================================
 */

#include <16F877A.h>
#fuses XT, NOWDT, PUT, BROWNOUT, NOLVP
#use delay(clock=4000000)

// ============================================================
// CONSTANTES
// ============================================================
#define NIVEL_MAX       5
#define NUM_VELOCIDADES 3

/*
 * VELOCIDADES DEL MOTOR PASO A PASO
 * ==================================
 * Unidad: milisegundos entre pasos (loop base = 1ms).
 * Configuracion asumida: A4988 en FULL-STEP (MS1=MS2=MS3=GND).
 * Motor tipico NEMA 17 o NEMA 23: 200 pasos por vuelta.
 *
 *   UMBRAL_VEL1 = 500ms -> 2 pasos/seg ->  0.6 RPM  (lenta, arranque seguro)
 *   UMBRAL_VEL2 = 200ms -> 5 pasos/seg ->  1.5 RPM  (media)
 *   UMBRAL_VEL3 =  80ms ->12 pasos/seg ->  3.6 RPM  (rapida, aun conservadora)
 *
 * Estos valores garantizan que el motor arranque sin perdida de pasos.
 * La diferencia entre velocidades es claramente visible en el eje.
 *
 * COMO AJUSTAR SI EL MOTOR NO GIRA:
 *   - Vibra sin girar (va muy rapido) -> aumentar los 3 umbrales
 *   - Quieres mas velocidad maxima    -> reducir UMBRAL_VEL3 con cuidado
 *   - Microstepping 1/16 activado     -> multiplicar todos x16
 *
 * COMO VER PASOS EN PROTEUS:
 *   Usar osciloscopio virtual en RC0 (pulsos de 5us, no visibles en LED).
 *   Para ver en LED temporalmente: cambiar los 3 umbrales a 500.
 */
#define UMBRAL_VEL1     500
#define UMBRAL_VEL2     200
#define UMBRAL_VEL3      80

// Tiempo de debounce en ms
#define DEBOUNCE_MS      50

// ============================================================
// VARIABLES GLOBALES
// ============================================================
int   velocidad_actual = 1;
int   nivel_actual     = 0;
int1  bomba_estado     = 0;
int16 ticks_motor      = 0;

/*
 * Estado de la maquina de estados del boton:
 *   0 = esperando pulsacion (boton suelto)
 *   1 = boton presionado, esperando confirmar debounce
 *   2 = pulsacion confirmada, esperando que se suelte
 *   3 = boton suelto tras pulsacion, esperando debounce de suelta
 */
int  btn_estado        = 0;
int16 btn_contador     = 0;     // Contador de ms para debounce

// ============================================================
// FUNCION: leer_nivel
// ============================================================
/*
 * Lee sensores independientes en RA0-RA3 y RA5.
 * Retorna el nivel mas alto detectado (0 = sin agua).
 */
int leer_nivel() {
    int ra = input_a();

    if (ra & 0x20) return 5;    // RA5 = nivel 5
    if (ra & 0x08) return 4;    // RA3 = nivel 4
    if (ra & 0x04) return 3;    // RA2 = nivel 3
    if (ra & 0x02) return 2;    // RA1 = nivel 2
    if (ra & 0x01) return 1;    // RA0 = nivel 1
    return 0;
}

// ============================================================
// FUNCION: controlar_bomba
// ============================================================
/*
 * Nivel 0          -> encender bomba (tanque vacio)
 * Nivel 5 (MAX)    -> apagar bomba
 * Niveles 1-4      -> mantener estado anterior
 */
void controlar_bomba(int nivel) {
    if (nivel == 0) {
        bomba_estado = 1;
        output_high(PIN_C2);
    }
    else if (nivel >= NIVEL_MAX) {
        bomba_estado = 0;
        output_low(PIN_C2);
    }
    // niveles 1-4: sin cambio
}

// ============================================================
// FUNCION: actualizar_leds
// ============================================================
void actualizar_leds(int vel) {
    output_low(PIN_B4);
    output_low(PIN_B5);
    output_low(PIN_B6);
    switch (vel) {
        case 1: output_high(PIN_B4); break;
        case 2: output_high(PIN_B5); break;
        case 3: output_high(PIN_B6); break;
    }
}

// ============================================================
// FUNCION: actualizar_display_bcd
// ============================================================
/*
 * Escribe el nivel en RB0-RB3 (BCD para 74LS47) sin alterar
 * RB4-RB6 (LEDs). Reconstruye el nibble alto desde
 * velocidad_actual para no depender de input_b().
 */
void actualizar_display_bcd(int nivel) {
    int nibble_alto = 0x00;
    if      (velocidad_actual == 1) nibble_alto = 0x10;
    else if (velocidad_actual == 2) nibble_alto = 0x20;
    else if (velocidad_actual == 3) nibble_alto = 0x40;

    output_b(nibble_alto | (nivel & 0x0F));
}

// ============================================================
// FUNCION: manejar_boton  (MAQUINA DE ESTADOS NO BLOQUEANTE)
// ============================================================
/*
 * Se llama UNA VEZ por iteracion del loop (~1ms).
 * NUNCA usa while() ni delay() internamente.
 * El loop principal siempre sigue corriendo.
 *
 * Estados:
 *
 *   [0] IDLE: boton suelto.
 *       Si detecta RC3=0 -> inicia contador, va a estado 1.
 *
 *   [1] DEBOUNCE PRESION: esperando DEBOUNCE_MS para confirmar.
 *       Si el contador llega y RC3 sigue en 0 -> cambiar velocidad,
 *       ir a estado 2.
 *       Si RC3 vuelve a 1 antes -> era ruido, volver a estado 0.
 *
 *   [2] ESPERANDO SUELTA: boton confirmado presionado.
 *       Esperar a que RC3 vuelva a 1 -> ir a estado 3.
 *
 *   [3] DEBOUNCE SUELTA: esperando DEBOUNCE_MS tras soltar.
 *       Al terminar -> volver a estado 0 (listo para proxima pulsacion).
 */
void manejar_boton() {
    switch (btn_estado) {

        case 0: // Esperando pulsacion
            if (!input(PIN_C3)) {
                btn_contador = 0;
                btn_estado   = 1;
            }
            break;

        case 1: // Debounce de presion
            btn_contador++;
            if (input(PIN_C3)) {
                // Boton solto antes del debounce: era ruido
                btn_estado = 0;
            }
            else if (btn_contador >= DEBOUNCE_MS) {
                // Pulsacion real confirmada: cambiar velocidad
                velocidad_actual++;
                if (velocidad_actual > NUM_VELOCIDADES)
                    velocidad_actual = 1;

                ticks_motor = 0;
                actualizar_leds(velocidad_actual);
                // Actualizar display para reflejar nuevo nibble de LEDs
                actualizar_display_bcd(nivel_actual);

                btn_estado = 2;
            }
            break;

        case 2: // Esperando que se suelte el boton
            if (input(PIN_C3)) {
                btn_contador = 0;
                btn_estado   = 3;
            }
            break;

        case 3: // Debounce de suelta
            btn_contador++;
            if (!input(PIN_C3)) {
                // Volvio a presionar durante debounce: ruido, seguir esperando
                btn_contador = 0;
            }
            else if (btn_contador >= DEBOUNCE_MS) {
                btn_estado = 0;  // Listo para la proxima pulsacion
            }
            break;
    }
}

// ============================================================
// FUNCION: paso_motor
// ============================================================
/*
 * Genera pulsos STEP usando ticks_motor como contador de ms.
 * Se detiene si nivel == NIVEL_MAX.
 *
 * En TEST_MOTOR=1: RC0 alterna cada 500ms (visible en Proteus).
 * En TEST_MOTOR=0: pulso de 5us (usar osciloscopio en Proteus).
 */
void paso_motor() {
    int16 umbral;

    // Detener motor si el tanque esta lleno
    if (nivel_actual >= NIVEL_MAX) {
        output_low(PIN_C0);
        ticks_motor = 0;
        return;
    }

    switch (velocidad_actual) {
        case 1:  umbral = UMBRAL_VEL1; break;
        case 2:  umbral = UMBRAL_VEL2; break;
        case 3:  umbral = UMBRAL_VEL3; break;
        default: umbral = UMBRAL_VEL1; break;
    }

    ticks_motor++;
    if (ticks_motor >= umbral) {
        ticks_motor = 0;
        // Pulso STEP: 5us es suficiente para el A4988 (minimo 1us segun datasheet)
        output_high(PIN_C0);
        delay_us(5);
        output_low(PIN_C0);
    }
}

// ============================================================
// MAIN
// ============================================================
void main() {

    // --- Configuracion de puertos ---
    set_tris_a(0x3F);       // RA0-RA5 entradas (sensores)
    set_tris_b(0x00);       // Puerto B todo salida (BCD + LEDs)
    set_tris_c(0x08);       // RC3 entrada (pulsador), resto salidas

    output_b(0x00);
    output_c(0x00);         // DIR=0, STEP=0, BOMBA=0

    // --- Deshabilitar ADC para usar RA0-RA5 como digitales ---
    setup_adc_ports(NO_ANALOGS);
    setup_adc(ADC_OFF);

    // --- Estado inicial de variables ---
    velocidad_actual = 1;
    bomba_estado     = 0;
    ticks_motor      = 0;
    btn_estado       = 0;
    btn_contador     = 0;

    actualizar_leds(velocidad_actual);

    /*
     * PRIMERA LECTURA REAL antes de entrar al loop.
     * Esto evita que la bomba arranque siempre apagada
     * independientemente del nivel real del tanque.
     * Si al encender el PIC el tanque ya esta vacio (nivel 0),
     * la bomba debe encender de inmediato.
     */
    nivel_actual = leer_nivel();
    actualizar_display_bcd(nivel_actual);
    controlar_bomba(nivel_actual);

    // --- Loop principal (~1ms por iteracion) ---
    while (TRUE) {
        nivel_actual = leer_nivel();           // 1. Leer sensores
        actualizar_display_bcd(nivel_actual);  // 2. Actualizar display
        controlar_bomba(nivel_actual);         // 3. Control bomba
        manejar_boton();                       // 4. Boton (no bloqueante)
        paso_motor();                          // 5. Motor paso a paso
        delay_ms(1);                           // 6. Base de tiempo: 1ms
    }
}
