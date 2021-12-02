#include <creacionEnviosMQTTyCAN.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32CAN.h>
#include <CAN_config.h>
#include <iostream>
using namespace std;
/*
 *  OBJETOS
*/
EnvioCAN         CAN;
CAN_frame_t      rx_frame;
CAN_device_t     CAN_cfg;

//WiFiClient       client;
//PubSubClient client(espClient);

EnvioMqtt   cadenaMqtt;

/*
 * FUNCIONES PROTOTIPO
*/
void envioCAN(String cadenaTCP);

/*
 * VARIABLES Mqtt
*/

// Update these with values suitable for your network.
const char* ssid = "RED ACCESA";
const char* password = "037E32E7";
char path[] = "/";                    //no tiene otras direcciones 
const char* mqtt_server = "192.168.1.115"; 

WiFiClient espClient;
PubSubClient client(espClient);

/*
 * VARIABLES GLOBALES
*/
byte    limiteEnvioInst = 0;
bool    banderaEscInst = false, banderaEscDia = false, banderaEscFecha = false;
bool    banderaFechaFin = false, configurarEntrada = false;
String  dataEscenario, dataConfiguracion;
char    dataSnd[15];

long lastMsg = 0;
char msg[50];
int value = 0;
int BUILTIN_LED = 15;
int BUILTIN_RELE1 = 32;
int BUILTIN_RELE2 = 33;

/*
 * ********************************************************************
 * Setup
 * ********************************************************************
*/

void setup() {
  pinMode(BUILTIN_RELE1, OUTPUT);
  pinMode(BUILTIN_RELE2, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.publish("Paco/Can","Mensaje Recibido");
  
  /*
   * Conexion CAN
  */
  /* Seleccionar pines TX, RX y baudios */
  CAN_cfg.speed=CAN_SPEED_125KBPS;
  CAN_cfg.tx_pin_id = GPIO_NUM_5;
  CAN_cfg.rx_pin_id = GPIO_NUM_35;
  
  /* Crear una cola de 10 mensajes en el buffer del CAN */
  CAN_cfg.rx_queue = xQueueCreate(300,sizeof(CAN_frame_t));
  
  //INICIALIZAR MODULO CAN
  ESP32Can.CANInit();

  
}

/*
 * ********************************************************************
 * Setup WIFI
 * ********************************************************************
*/

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/*
 * ********************************************************************
 * Botton y Rele
 * ********************************************************************
*/

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();


  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_RELE1, HIGH);  
    client.publish("Paco/cocina/foco","Encendido");
  }
   else if ((char)payload[0] == '0') {
    digitalWrite(BUILTIN_RELE1, LOW); 
    client.publish("Paco/cocina/foco","Apagado");
  }


}

/*
 * ********************************************************************
 * Reconectado
 * ********************************************************************
*/

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESPClient")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("Paco/Can", "Enviando el primer mensaje");
      // ... and resubscribe
      client.subscribe("Paco/Can");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

}

/*
 * ********************************************************************
 * Programa principal
 * ********************************************************************
*/

void loop() {
  String data;
  
  /*
   * MENSAJES RECIBIDOS POR EL BUS CAN
   */
  if(xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 3*portTICK_PERIOD_MS)==pdTRUE){
      
      if(rx_frame.FIR.B.FF==CAN_frame_std)
        printf("New standard frame");
      else
        printf("NUEVO MENSAJE CAN     ");

      if(rx_frame.FIR.B.RTR==CAN_RTR)
        printf(" RTR from 0x%.3x, DLC %d\r\n",rx_frame.MsgID,  rx_frame.FIR.B.DLC);
      else{
        printf("ID CAN: 0x%.3x, No. BYTES: %d\n",rx_frame.MsgID,  rx_frame.FIR.B.DLC);
        Serial.print(char(rx_frame.data.u8[0]));Serial.print("|");
        Serial.print(rx_frame.data.u8[1]);Serial.print("|");
        Serial.print(rx_frame.data.u8[2]);Serial.print("|");
        Serial.print(rx_frame.data.u8[3]);Serial.print("|");
        Serial.print(rx_frame.data.u8[4]);Serial.print("|");
        Serial.print(rx_frame.data.u8[5]);Serial.print("|");
        Serial.print(rx_frame.data.u8[6]);Serial.print("|");
        Serial.println(rx_frame.data.u8[7]);
        if(rx_frame.MsgID == 255 || 1){
          switch(char(rx_frame.data.u8[0])){
            case '0': 
                      Serial.println("ACTIVACIÓN/DESACTIVACIÓN CONFIRMADA");
                      cadenaMqtt.envioActivacion(&rx_frame, dataSnd);
                      client.publish("Paco/Can", dataSnd);
                      
                      //MqttClient.sendData(dataSnd);
                      
                      break;
            
            default:  break;       
          }
        memset(dataSnd, 'x', 15);
        printf("\n");
        }
      }
  }

/*
 * ********************************************************************
 * Envio  MQTT
 * ********************************************************************
*/

  
  if (!client.connected()) {
  
    reconnect();
    
  }
  
  client.loop();
  //long now = millis();
  //if (now - lastMsg > 3000) {
    //lastMsg = now;
    //++value;
    //snprintf (msg, 75, "Escuchando #%ld", value);
    //snprintf = print normal, value = tamaño del string
    //Serial.print("Publish message: ");
    //Serial.println(msg);
    //client.publish("Paco/Can", msg);
    
    //client.publish("Paco/cocina/refri", dataSnd);
  //}

 


}

/*
 * ********************************************************************
 * Envio CAN
 * ********************************************************************
*/

void envioCAN(String cadenaTCP){
  
  char destinatario[3];
  byte destinatarioByte;
  
  for(byte i=0; i<2; i++) destinatario[i] = cadenaTCP[3+i];
  destinatarioByte = CAN.x2i(destinatario);
  rx_frame.FIR.B.FF = CAN_frame_ext;
  rx_frame.MsgID = destinatarioByte;
  rx_frame.FIR.B.DLC = 8;
  //ENVIO CAN
  ESP32Can.CANWriteFrame(&rx_frame);
}
