#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// PINES
// =====================================================

#define FAN_PIN     6
#define RELAY_PIN   5
#define BUZZER_PIN  9
#define BUT1_PIN    7
#define BUT2_PIN    8
#define BUT3_PIN    10
#define BUT4_PIN    11
#define LEVER_PIN    12

// OLED 0.91" normalmente 128x32
#define SCREEN_WIDTH  120
#define SCREEN_HEIGHT 32

#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// CONFIGURACIÓN
// =====================================================

const unsigned long FAN_INTERVAL = 5000;  // 5 segundos
const unsigned long BUZZER_TIME   = 150;   // duración pitido

bool fanState = false;
bool relayState = false;

unsigned long lastChange = 0;

// =====================================================
// BUZZER PWM
// =====================================================

void buzzerBeep()
{
  // PWM al 50% en D6
  // analogWrite() genera PWM ~490 Hz en Arduino Uno/Nano
  analogWrite(BUZZER_PIN, 128);

  delay(BUZZER_TIME);

  analogWrite(BUZZER_PIN, 0);
}

void readbuttons()
{

}

// =====================================================
// OLED
// =====================================================

void updateDisplay()
{
  unsigned long elapsed = millis() - lastChange;

  unsigned long remaining;

  if (elapsed >= FAN_INTERVAL)
    remaining = 0;
  else
    remaining = (FAN_INTERVAL - elapsed) / 1000 + 1;

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  // Línea 1
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("PCB HEATED BED V2");

  // Línea 2
  display.setTextSize(2);
  display.setCursor(0, 12);

  if (relayState)
    display.print("BED ON");
  else
    display.print("BED OFF");

  // Tiempo
  display.setTextSize(1);
  display.setCursor(85, 19);
  display.print(remaining);
  display.print("s");

  // Línea inferior
  display.setCursor(0, 26);
  display.print("MOSFET:D5  BUZZER:D6");

  display.display();
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  pinMode(FAN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // Inicialmente ventilador apagado
  digitalWrite(FAN_PIN, LOW);
  analogWrite(BUZZER_PIN, 0);
  digitalWrite(RELAY_PIN, LOW);

  // Inicializar OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    // Si la OLED no responde, dejamos el programa aquí
    while (true)
    {
      digitalWrite(BUZZER_PIN, 0);
      delay(1000);
    }
  }

  // Pantalla inicial
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(15, 0);
  display.print("PCB Heated Bed V2");

  display.display();

  delay(2000);

  // Estado inicial
  fanState = false;
  relayState = false;
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  lastChange = millis();

  updateDisplay();
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  unsigned long currentTime = millis();

  // Actualizar OLED continuamente
  updateDisplay();
  readbuttons();

  // ===================================================
  // CAMBIAR FAN CADA 5 SEGUNDOS
  // ===================================================

  if (currentTime - lastChange >= FAN_INTERVAL)
  {
    // Cambiar estado
    fanState = !fanState;

    // Activar/desactivar MOSFET
    digitalWrite(FAN_PIN, fanState ? HIGH : LOW);

    // Pitido
    buzzerBeep();

    // Reiniciar temporizador
    lastChange = millis();

    // Actualizar inmediatamente
    updateDisplay();
  }

  delay(50);
}