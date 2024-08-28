/**
 * @file ojos_dobby.ino
 * @brief Programa para controlar los ojos de Dobby con ESP32
 * @version 2.1
 * @date 2024-08-16
 * 
 * @copyright Copyright (c) 2024
 * 
 */

const char *version = "2.2"; // Se agrega modo dormir y botón de inicio y se sacó Reset
#include <ESP32Servo.h> // Incluir la librería ESP32Servo ESP32Servo con versión 3.0.5 con esp32\2.0.17 con versión 3.0.? no compilará.
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//-----------------------------Includes de audio
#include "AudioFileSourceSD.h"   // Biblioteca para fuente de audio desde la tarjeta SD
#include "AudioGeneratorMP3.h"   // Biblioteca para generar audio MP3
#include "AudioOutputI2SNoDAC.h" // Biblioteca para salida de audio sin DAC
#include "FS.h"                  // Biblioteca del sistema de archivos
#include "SD.h"                  // Biblioteca para tarjeta SD
#include "SPI.h"                 // Biblioteca para interfaz SPI
// Pines del Bus SPI para la conexión de la Tarjeta SD
#define SCK 18   // Pin de reloj para SPI
#define MISO 19  // Pin de salida de datos para SPI
#define MOSI 23  // Pin de entrada de datos para SPI
#define CS 5     // Pin de selección de chip para SPI

#define PIN_BOTON_INICIAR 34

// Variables para el manejo del audio
AudioGeneratorMP3 *mp3;       // Generador de audio MP3
AudioFileSourceSD *fuente;    // Fuente de archivo de audio desde la tarjeta SD
AudioOutputI2SNoDAC *salida;  // Salida de audio sin DAC
int totalRespuestasAleatorias = 3; //Cantidad de archivos de audio que posee para respuestas aleatorias 
//#include <TickTwo.h>
bool OTAhabilitado = false; // variable que se utilizara para inabilitar la función OTA si no se pudo lograr la conexion WIFI
// Configuración de la red WiFi
const char *ssid = "";          // Nombre de la red WiFi
const char *password = ""; // Contraseña de la red WiFi

// Definir los pines donde están conectados los servos
const int servoPin1 = 14; // Servo del párpado superior izquierdo. Abre sentido Anti  Horario
const int servoPin2 = 27; // Servo del párpado superior derecho .Abre sentido Horario
const int servoPin3 = 26; // Servo del párpado inferior izquierdo. Abre sentido Horario
const int servoPin4 = 25; // Servo del párpado inferior derecho. abre sentido Antihorario
const int servoPin5 = 33; // Servo de movimiento horizontal de ambos ojos +- 15º
const int servoPin6 = 32; // Servo de movimiento vertical de ambos ojos +-15º

const int centroVertical = 90; //posicion central de movimiento vertical de ambos ojos 
const int recorridoVertical = 18; // movimiento vertical de ambos ojos desde el centro +-
const int centroHorizontal = 90; //posicion central de movimiento Horizontal de ambos ojos 
const int recorridoHorizontal = 35; // movimiento Izq a Der de ambos ojos desde el centro +- en Grados

const int centroParpadoInfIzq = 90; //posicion central de parpado cerrado del parpado inferior izquierdo 
const int recorridoParpadoInfIzq = 45; // movimiento parpado Inferior Izquierdo
const int centroParpadoInfDer = 90; // posición central de parpado cerrado del párpado inferior derecho
const int recorridoParpadoInfDer = 45; // movimiento párpado Inferior Derecho
const int centroParpadoSupIzq = 90; // posición central de parpado cerrado del párpado superior izquierdo
const int recorridoParpadoSupIzq = 45; // movimiento párpado Superior Izquierdo
const int centroParpadoSupDer = 90; // posición central de parpado cerrado del párpado superior derecho
const int recorridoParpadoSupDer = 45; // movimiento párpado Superior Derecho



// Crear objetos Servo
Servo servo1; // Servo del párpado superior izquierdo
Servo servo2; // Servo del párpado superior derecho
Servo servo3; // Servo del párpado inferior izquierdo
Servo servo4; // Servo del párpado inferior derecho
Servo servo5; // Servo de movimiento horizontal de ambos ojos
Servo servo6; // Servo de movimiento vertical de ambos ojos

// Variables para el modo dormir
const unsigned long TIEMPO_ANTES_DE_DORMIR = 30000; //600000; // 10 minutos en milisegundos
const unsigned long TIEMPO_Entre_Audios = 10000; // 30 segundo en milisegundos

unsigned long ultimaActividad = 0;
unsigned long ActividadInicio = 0;

/**
 * @brief Configura los pines, servicios y realiza la inicialización del sistema
 */

void setup() {
  Serial.begin(115200); // Inicializa la comunicación serial a 115200 baudios
  Serial.println(version); 
  if (OTAhabilitado) initOTA(); // Inicia configuración OTA para programación en el aire.  
  
    if (!SD.begin(CS)) { // Inicializa la tarjeta SD conectada al pin de selección CS
    Serial.println("Tarjeta SD no encontrada"); // Mensaje de error si no se encuentra la tarjeta SD
    return; // Termina la configuración si no se encuentra la tarjeta SD
  }
    //crea insatancias para el manejo de MP3, Salida de Audio y SD
  mp3 = new AudioGeneratorMP3();      // Crea una instancia del generador de audio MP3
  salida = new AudioOutputI2SNoDAC(); // Crea una instancia de la salida de audio sin DAC
  fuente = new AudioFileSourceSD();   // Crea una instancia de la fuente de audio desde la tarjeta SD
 
  salida->SetOutputModeMono(true); // Configura la salida de audio en modo monoaural
    // Configurar pines de botones
  pinMode(PIN_BOTON_INICIAR, INPUT_PULLUP);
  
  // Configurar interrupción para despertar
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOTON_INICIAR, LOW);


  // Adjuntar los servos a los pines correspondientes
  // use enservo modelo SG90  min/max de 500us y 2400us00
  // para servos más grandes como  MG995, use 1000us y 2000us,
  servo1.attach(servoPin1, 500, 2400); 
  servo2.attach(servoPin2, 500, 2400);
  servo3.attach(servoPin3, 500, 2400);
  servo4.attach(servoPin4, 500, 2400);
  servo5.attach(servoPin5, 500, 2400);
  servo6.attach(servoPin6, 500, 2400);
  yield();
  TodosCentro(); //Pone todos los cervos en cero, ojos centrados y parpados cerrados
    Serial.println("Todo al centro"); 
  yield();
  reproducirIntroduccion();
    ultimaActividad = millis();
    ActividadInicio = ultimaActividad;
}

void loop() {
  OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja la actualización OTA, solo si la condición OTAhabilitado es Verdadera
 yield(); // Pasa el control a otras tareas
    
  if (millis() - ActividadInicio > TIEMPO_ANTES_DE_DORMIR) {
    if (!mp3->isRunning()) {
      entrarModoSueno();
      Serial.println("entrar en modo sueño");
    }
  }

 
  if (mp3->isRunning()) { // Verifica si el audio está en reproducción
    if (!mp3->loop() ) { // Si el audio ha terminado de reproducirse
      mp3->stop();         // Detiene la reproducción
      fuente->close();     // Cierra el archivo de audio
      Serial.println("Audio Stop"); // Mensaje de parada de audio
      Serial.println("Archivo Cerrado"); // Mensaje de archivo cerrado
      yield(); 
    }
  } else { // Si el audio no está en reproducciónservo1.attach(servoPin1, 500, 2400);
  abrirParpados();
  Serial.println("Abrir Parpados"); 
  yield();
  delay(2000);
  OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja la actualización OTA, solo si la condición OTAhabilitado es Verdadera
  mirarDerecha();
  Serial.println("Mirar derecha"); yield();
  delay(2000);
  OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja la actualización OTA, solo si la condición OTAhabilitado es Verdadera
  mirarCentro();
  OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja la actualización OTA, solo si la condición OTAhabilitado es Verdadera
  Serial.println("Mirar al centro"); yield();
  delay(2000);
  mirarIzquierda(); OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja la actualización OTA, solo si la condición OTAhabilitado es Verdadera
  Serial.println("mirar a la izquierda"); yield();
  delay(2000); OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja la actualización OTA, solo si la condición OTAhabilitado es Verdadera
  Serial.println("Mirar al centro"); yield();
  mirarCentro();
  Serial.println("guiñar ojo DERECHO "); yield();
  guinarOjoDerecho(); OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja la actualización OTA, solo si la condición OTAhabilitado es Verdadera
  delay(2000);
  Serial.println("Mirar arriba"); yield();
  mirarArriba(); OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja la actualización OTA, solo si la condición OTAhabilitado es Verdadera
  delay(2000);
  Serial.println("Mirar abajo"); yield();
  mirarAbajo(); OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja la actualización OTA, solo si la condición OTAhabilitado es Verdadera
  delay(1000);
  Serial.println("Mirar al centro"); yield();
  mirarCentro(); OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja la actualización OTA, solo si la condición OTAhabilitado es Verdadera
  delay(1000);
  cerrarParpados();
 
 if (millis() - ultimaActividad > TIEMPO_Entre_Audios) {
    
      Serial.println("reproducirRespuestaAleatoria"); 
      reproducirRespuestaAleatoria();
      
    
  }
  

  //movimientoCircularHorario();

   }
}

void TodosCentro(){
  servo1.write(centroParpadoSupIzq);  // cerrar párpado superior izquierdo
  servo2.write(centroParpadoSupDer);  // cerrar párpado superior derecho
  servo3.write(centroParpadoInfIzq);  // cerrar párpado inferior izquierdo
  servo4.write(centroParpadoInfDer);  // cerrar párpado inferior derecho
  servo5.write(centroHorizontal);     // centrar de movimiento horizontal de ambos ojos
  servo6.write(centroVertical);       // centrar de movimiento vertical de ambos ojos
}

// Método para abrir los párpados
void abrirParpados() {
  servo1.write(centroParpadoSupIzq - recorridoParpadoSupIzq);  // abrir párpado superior izquierdo 
  servo3.write(centroParpadoInfIzq + recorridoParpadoInfIzq);  // abrir párpado inferior izquierdo
  servo2.write(centroParpadoSupDer + recorridoParpadoSupDer);  // abrir párpado superior derecho 
  servo4.write(centroParpadoInfDer - recorridoParpadoInfDer);  // abrir párpado inferior derecho 
}

// Método para cerrar los párpados
void cerrarParpados() {
  servo1.write(centroParpadoSupIzq);  // cerrar párpado superior izquierdo
  servo3.write(centroParpadoInfIzq);  // cerrar párpado inferior izquierdo
  servo2.write(centroParpadoSupDer);  // cerrar párpado superior derecho
  servo4.write(centroParpadoInfDer);  // cerrar párpado inferior derecho
}
// Método para mirar arriba
void mirarArriba() {
  servo6.write(centroVertical - recorridoVertical); // Mover ambos ojos hacia arriba desde el centro mueve antihorario para subir
}

// Método para mirar abajo
void mirarAbajo() {
  servo6.write(centroVertical + recorridoVertical); // Mover ambos ojos hacia arriba desde el centro mueve horario para bajar
}

// Método para mirar a la izquierda
void mirarIzquierda() {
  servo5.write(centroHorizontal + recorridoHorizontal); // Mover ambos ojos hacia la izquierda horario
}

// Método para mirar a la derecha
void mirarDerecha() {
  servo5.write(centroHorizontal - recorridoHorizontal); // Mover ambos ojos hacia la derecha anti Horario
}

// Método para guiñar el ojo izquierdo
void guinarOjoIzquierdo() {
  servo1.write(centroParpadoInfIzq); // Cerrar párpado superior izquierdo
  servo3.write(centroParpadoInfIzq);  // cerrar párpado inferior izquierdo
  delay(200); // Esperar medio segundo
  servo1.write(centroParpadoSupIzq - recorridoParpadoSupIzq);  // abrir párpado superior izquierdo 
  servo3.write(centroParpadoInfIzq + recorridoParpadoInfIzq);  // abrir párpado inferior izquierdo

}

// Método para guiñar el ojo derecho
void guinarOjoDerecho() {
  servo2.write(centroParpadoSupDer);  // cerrar párpado superior derecho
  servo4.write(centroParpadoInfDer);  // cerrar párpado inferior derecho

  delay(200); // Esperar medio segundo
  servo2.write(centroParpadoSupDer + recorridoParpadoSupDer);  // abrir párpado superior derecho 
  servo4.write(centroParpadoInfDer - recorridoParpadoInfDer);  // abrir párpado inferior derecho 
}
// Método para mirar al centro
void mirarCentro() {
  mirarCentroHorizontal();
  mirarCentroVertical();
}

// Método para centrar movimiento vertical de ambos ojos
void mirarCentroVertical() {
  servo6.write(centroVertical); // Centrar movimiento vertical de ambos ojos
}

// Método para centrar movimiento horizontal de ambos ojos
void mirarCentroHorizontal() {
  servo5.write(centroHorizontal); // Centrar movimiento horizontal de ambos ojos
}

// Método para mirar arriba a la izquierda
void mirarArribaIzq() {
  servo5.write(centroHorizontal - recorridoHorizontal); // Mover ojos a la izquierda
  servo6.write(centroVertical - recorridoVertical); // Mover ojos hacia arriba
}

// Método para mirar arriba a la derecha
void mirarArribaDer() {
  servo5.write(centroHorizontal + recorridoHorizontal); // Mover ojos a la derecha
  servo6.write(centroVertical - recorridoVertical); // Mover ojos hacia arriba
}

// Método para mirar abajo a la izquierda
void mirarAbajoIzq() {
  servo5.write(centroHorizontal + recorridoHorizontal); // Mover ojos a la izquierda
  servo6.write(centroVertical - recorridoVertical); // Mover ojos hacia abajo
}

// Método para mirar abajo a la derecha
void mirarAbajoDer() {
  servo5.write(centroHorizontal + recorridoHorizontal); // Mover ojos a la derecha
  servo6.write(centroVertical + recorridoVertical); // Mover ojos hacia abajo
}

// Método para movimiento circular de los ojos sentido horario
void movimientoCircularHorario() {
  for (int i = 0; i <= recorridoHorizontal; i++) {
    servo5.write(centroHorizontal + i);
    delay(50);
  }
  for (int i = 0; i <= recorridoVertical; i++) {
    servo6.write(centroVertical + i);
    delay(50);
  }
  for (int i = recorridoHorizontal; i >= 0; i--) {
    servo5.write(centroHorizontal + i);
    delay(50);
  }
  for (int i = recorridoVertical; i >= 0; i--) {
    servo6.write(centroVertical + i);
    delay(50);
  }
}

// Método para movimiento circular de los ojos sentido antihorario
void movimientoCircularAntihorario() {
  for (int i = 0; i <= recorridoHorizontal; i++) {
    servo5.write(centroHorizontal - i);
    delay(50);
  }
  for (int i = 0; i <= recorridoVertical; i++) {
    servo6.write(centroVertical - i);
    delay(50);
  }
  for (int i = recorridoHorizontal; i >= 0; i--) {
    servo5.write(centroHorizontal - i);
    delay(50);
  }
  for (int i = recorridoVertical; i >= 0; i--) {
    servo6.write(centroVertical - i);
    delay(50);
  }
}

void reproducirAudio(const char *ruta) {
  if (!SD.exists(ruta)) { // Verifica si el archivo existe en la tarjeta SD
    Serial.println(ruta);
    Serial.println("Archivo no encontrado"); // Mensaje de error si el archivo no existe
    return; // Termina la función si el archivo no existe
  }

  if (!fuente->open(ruta)) { // Abre el archivo de audio
     Serial.println(ruta);
    Serial.println("Error al abrir el archivo"); // Mensaje de error si no se puede abrir el archivo
    return; // Termina la función si no se puede abrir el archivo
  }

  yield(); // Pasa el control a otras tareas
  Serial.println("Reproduciendo :");
  Serial.println(ruta);
  mp3->begin(fuente, salida); // Inicia la reproducción del audio
  ultimaActividad = millis();
}

void reproducirRespuestaAleatoria() {
  char ruta[15];
  int numeroRespuesta = random(1, totalRespuestasAleatorias); // Genera un número aleatorio entre 1 total de audios
  snprintf(ruta, sizeof(ruta), "/resp%d.mp3", numeroRespuesta); // Formatea la ruta del archivo de audio de la respuesta aleatoria
  Serial.print("Aleatorea: ");
  Serial.println(ruta);
  reproducirAudio(ruta); // Llama a la función de reproducción de audio genérica
}

void reproducirIntroduccion() {
  const char *archivoIntroduccion = "/intro.mp3"; // Ruta del archivo de introducción
  reproducirAudio(archivoIntroduccion); // Llama a la función de reproducción de audio genérica
  while (mp3->isRunning()){
    if (!mp3->loop()) { // Si el audio ha terminado de reproducirse
      mp3->stop();         // Detiene la reproducción
      fuente->close();     // Cierra el archivo de audio
      Serial.println("Audio Stop"); // Mensaje de parada de audio
      Serial.println("Archivo Cerrado"); // Mensaje de archivo cerrado
      yield(); // Pasa el control a otras tareas
    }
  }
   return;
}

/**
 * @brief Entra en modo de bajo consumo
 */
void entrarModoSueno() {
  Serial.println("Entrando en modo sueño...");
  cerrarParpados();
  delay(1000);
  esp_deep_sleep_start();
}



void initOTA() {
  // Conexión a la red WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a la red WiFi");
  ArduinoOTA.setHostname("OjosDobby");
  ArduinoOTA.setPassword("O");
  // Configuración de OTA
  ArduinoOTA.onStart([]() {
    String tipo;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      tipo = "sketch";
    } else { // U_SPIFFS
      tipo = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Inicio de actualización OTA: " + tipo);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nFin de la actualización OTA");
  });

  ArduinoOTA.onProgress([](unsigned int progreso, unsigned int total) {
    Serial.printf("Progreso: %u%%\r", (progreso / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error [%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Error de autenticación");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Error de inicio");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Error de conexión");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Error de recepción");
    } else if (error == OTA_END_ERROR) {
      Serial.println("Error de finalización");
    }
  });

  ArduinoOTA.begin();
  Serial.println("Listo para actualización OTA");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
