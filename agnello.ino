#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// ======== DEFINIÇÕES DE PINOS ========
#define LED_VERM 17
#define LED_VERD 15
#define LED_AM 5
#define LDR 34         
#define DHTPIN 4
#define DHTTYPE DHT22
#define BUZZ 19

// ======== LCD CONFIG ========
#define I2C_ADDR 0x27
#define LCD_COLUMNS 16
#define LCD_LINES 2
LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_LINES);

// ======== DHT ========
DHT dht(DHTPIN, DHTTYPE);

// ======== CONSTANTES ========
const float GAMMA = 0.7;
const float RL10 = 50.0; 
const int meia_luz = 60;
const int muito_claro = 100;

// ======== WI-FI E MQTT CONFIG ========
const char* default_SSID = "Wokwi-GUEST"; 
const char* default_PASSWORD = ""; 
const char* default_BROKER_MQTT = "107.22.140.214"; 
const int default_BROKER_PORT = 1883; 
const char* default_TOPICO_SUBSCRIBE = "/TEF/device007/cmd"; 
const char* default_TOPICO_PUBLISH_1 = "/TEF/device007/attrs"; 
const char* default_TOPICO_PUBLISH_2 = "/TEF/device007/attrs/l"; 
const char* default_TOPICO_PUBLISH_3 = "/TEF/device007/attrs/t"; 
const char* default_TOPICO_PUBLISH_4 = "/TEF/device007/attrs/u"; 
const char* default_ID_MQTT = "fiware_001"; 
const char* topicPrefix = "device007";

// Variáveis para configurações editáveis
char* SSID = const_cast<char*>(default_SSID);
char* PASSWORD = const_cast<char*>(default_PASSWORD);
char* BROKER_MQTT = const_cast<char*>(default_BROKER_MQTT);
int BROKER_PORT = default_BROKER_PORT;
char* TOPICO_SUBSCRIBE = const_cast<char*>(default_TOPICO_SUBSCRIBE);
char* TOPICO_PUBLISH_1 = const_cast<char*>(default_TOPICO_PUBLISH_1);
char* TOPICO_PUBLISH_2 = const_cast<char*>(default_TOPICO_PUBLISH_2);
char* TOPICO_PUBLISH_3 = const_cast<char*>(default_TOPICO_PUBLISH_3);
char* TOPICO_PUBLISH_4 = const_cast<char*>(default_TOPICO_PUBLISH_4);
char* ID_MQTT = const_cast<char*>(default_ID_MQTT);

WiFiClient espClient;
PubSubClient MQTT(espClient);

char EstadoSaida = '0';

// ======== VARIÁVEIS ========
float voltage, resistance, lux, temp, umid, mediaTemp, mediaUmid, somaTemp, somaUmid;
int lum, contadorMensagens, lum_ldr;
int leituras = 5;

#define PRIORIDADE_NORMAL 0
#define PRIORIDADE_MEDIA 1
#define PRIORIDADE_ALTA 2

static int prioridadeAtual = PRIORIDADE_NORMAL;
bool alarmeLuminosidade = false;
bool alarmeTemperatura = false;
bool alarmeUmidade = false;

unsigned long tempoAtual = 0;
unsigned long ultimaLeituraSensor = 0;
unsigned long ultimoUpdateRapido = 0;
unsigned long intervaloLeitura = 3000;
const unsigned long intervaloRapido = 200;
unsigned long tempoMensagemLCD = 3000;
bool mostrandoMensagem = false;
String mensagemAtualLinha1 = "";
String mensagemAtualLinha2 = "";
unsigned long tempoExibicaoMensagem = 0;
bool mensagemPrioritaria = false;
bool condicaoNormal = true;
bool alarmeLigado = false;

bool controleManual = false;
unsigned long tempoUltimoComando = 0;
const unsigned long TEMPO_RESET_MANUAL = 100000;

struct Mensagem {
  String linha1;
  String linha2;
  int prioridade;
};

Mensagem mensagensPrioritarias[4];

byte taca1[8] = {
  B11111,
  B10000,
  B11111,
  B11111,
  B01111,
  B00111,
  B00001,
  B00001
  };

byte taca2[8] = {
  B11111,
  B00001,
  B11111,
  B11111,
  B11110,
  B11100,
  B10000,
  B10000
};

byte taca3[8] = {
  B00001,
  B00001,
  B00001,
  B00001,
  B00001,
  B01111,
  B00000,
  B00000
};

byte taca4[8] = {
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B11110,
  B00000,
  B00000
};


// ======== FUNÇÕES ========
void initWiFi() {
    delay(10);
    Serial.println("------Conexao WI-FI------");
    Serial.print("Conectando-se na rede: ");
    Serial.println(SSID);
    Serial.println("Aguarde");
    reconectWiFi();
}

void reconectWiFi() {
    if (WiFi.status() == WL_CONNECTED)
        return;
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("Conectado com sucesso na rede ");
    Serial.print(SSID);
    Serial.println("IP obtido: ");
    Serial.println(WiFi.localIP());
}

void initMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqtt_callback);
}

void reconnectMQTT() {
  while (!MQTT.connected()) {
    Serial.print("* Tentando conectar ao broker MQTT: ");
    Serial.println(BROKER_MQTT);
    if (MQTT.connect(ID_MQTT)) {
      Serial.println("Conectado ao broker MQTT!");
      MQTT.subscribe(TOPICO_SUBSCRIBE);
    } else {
      Serial.println("Falha na conexão, tentando novamente...");
      delay(2000);
    }
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (int i = 0; i < length; i++) {
        char c = (char)payload[i];
        msg += c;
    }
    Serial.print("- Mensagem recebida: ");
    Serial.println(msg);
    String onTopic = String(topicPrefix) + "@on|";
    String offTopic = String(topicPrefix) + "@off|";
    String autoTopic = String(topicPrefix) + "@auto|";

    if (msg.equals(onTopic)) {
        ligarBuzz();
        controleManual = true;
        EstadoSaida = '1';        
        Serial.println("Modo manual ativado → Buzzer LIGADO por comando MQTT");
    }

    if (msg.equals(offTopic)) {
        desligarBuzz();
        controleManual = true;
        EstadoSaida = '0';
        Serial.println("Modo manual ativado → Buzzer DESLIGADO por comando MQTT");
    }

    if (msg.equals(autoTopic)) {
       controleManual = false;
       Serial.println("Modo automatico ativado → Buzzer cotrolado por sensores");
    }
}

void VerificaConexoesWiFIEMQTT() {
    if (!MQTT.connected())
        reconnectMQTT();
    reconectWiFi();
}

void AtualizaEstadoSaida() {
  if (!controleManual){
    if (alarmeLigado) {
      ligarBuzz();
      EstadoSaida = '1';
    }else {
      desligarBuzz();
      EstadoSaida = '0';
    }
  }
}

void EnviaEstadoOutputMQTT() {
    AtualizaEstadoSaida();
    if (EstadoSaida == '1') {
        MQTT.publish(TOPICO_PUBLISH_1, "s|on");
    }

    if (EstadoSaida == '0') {
        MQTT.publish(TOPICO_PUBLISH_1, "s|off");
    }
    Serial.print("Estado enviado ao broker: ");
    Serial.println(EstadoSaida == '1' ? "ON" : "OFF");
    Serial.print("Modo atual: ");
    Serial.println(controleManual ? "MANUAL" : "AUTOMÁTICO");
}

void ligarBuzz() {
  if (!alarmeLigado) {
    tone(BUZZ, 1000);
    alarmeLigado = true;
  }
}

void desligarBuzz() {
  if (alarmeLigado) {
    noTone(BUZZ);
    alarmeLigado = false;
  }
}

// ======== SENSOR DE LUMINOSIDADE ========
void sensorLuminosidade(){
  lum_ldr = analogRead(LDR);
  lum = map(lum_ldr, 4095, 0, 1024, 0);
  voltage = lum / 1024.0 * 5;
  resistance = 2000 * voltage / (1 - voltage / 5); 
  lux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA));

  Serial.print("Leitura ADC: ");
  Serial.print(lum_ldr);
  Serial.print(" | Lum: ");
  Serial.print(lum);
  Serial.print(" | Lux: ");
  Serial.println(lux);

  String mensagemLumi = String(lux);
  MQTT.publish(default_TOPICO_PUBLISH_2, mensagemLumi.c_str());
}


// ======== SENSOR DHT22 ========
void sensorTempUmid(){     
  temp = dht.readTemperature();    
  umid = dht.readHumidity();                    
  
  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.print("°C | Umid: ");
  Serial.print(umid);
  Serial.print("%");

  String mensagemTemp = String(temp);
  MQTT.publish(default_TOPICO_PUBLISH_3, mensagemTemp.c_str());
  //String umidMsg = "umidade: " + String(umid);
  String mensagemUmid = String(umid);
  MQTT.publish(default_TOPICO_PUBLISH_4, mensagemUmid.c_str());
}

// ======== LCD ========
void atualizarLCD() {
  static unsigned long ultimaAtualizacao = 0;
  static String ultimaLinha1 = "";
  static String ultimaLinha2 = "";
  const unsigned long intervaloAtualizacao = 500;

 if (millis() - ultimaAtualizacao >= intervaloAtualizacao) {
    if (mensagemAtualLinha1 != ultimaLinha1 || mensagemAtualLinha2 != ultimaLinha2) {
      lcd.setCursor(0, 0);
      lcd.print("                "); 
      lcd.setCursor(0, 0);
      lcd.print(mensagemAtualLinha1);
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(mensagemAtualLinha2);

      ultimaLinha1 = mensagemAtualLinha1;
      ultimaLinha2 = mensagemAtualLinha2;
    }
    ultimaAtualizacao = millis();
  }
}

// ======== LÓGICA DE EXIBIÇÃO ========
void mostrarMensagem(String linha1, String linha2, int prioridade) {
  static int ultimaPrioridade = PRIORIDADE_NORMAL;

  if (prioridade >= PRIORIDADE_ALTA) {
    if (contadorMensagens < 4) {  
      mensagensPrioritarias[contadorMensagens].linha1 = linha1;
      mensagensPrioritarias[contadorMensagens].linha2 = linha2;
      mensagensPrioritarias[contadorMensagens].prioridade = prioridade;
      contadorMensagens++;
    }
  }

  static unsigned long tempoExibicao = 0;
  static bool exibindoMensagensPrioritarias = false;
  
  if (contadorMensagens > 0) {
    if (!exibindoMensagensPrioritarias) {
      exibindoMensagensPrioritarias = true;
      tempoExibicao = millis(); 
    }

    if (millis() - tempoExibicao >= tempoMensagemLCD) {
      mensagemAtualLinha1 = mensagensPrioritarias[0].linha1;
      mensagemAtualLinha2 = mensagensPrioritarias[0].linha2;
      atualizarLCD();
      for (int i = 0; i < contadorMensagens - 1; i++) {
        mensagensPrioritarias[i] = mensagensPrioritarias[i + 1];
      }

      contadorMensagens--;  
      tempoExibicao = millis(); 
    }
  }
  if (contadorMensagens == 0 && exibindoMensagensPrioritarias) {
    exibindoMensagensPrioritarias = false;  
  }

  if (contadorMensagens == 0 && !exibindoMensagensPrioritarias) {
    if (prioridade == PRIORIDADE_NORMAL) {
      mensagemAtualLinha1 = linha1;
      mensagemAtualLinha2 = linha2;
      atualizarLCD();
    }
  }
}

void setup(){
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");

  pinMode(LED_VERM, OUTPUT);
  pinMode(LED_AM, OUTPUT);
  pinMode(LED_VERD, OUTPUT);
  pinMode(BUZZ, OUTPUT);

  Serial.begin(115200);
  dht.begin();  

  lcd.createChar(0, taca1);
  lcd.createChar(1, taca2);
  lcd.createChar(2, taca3);
  lcd.createChar(3, taca4);

  initWiFi();
  initMQTT();
  MQTT.publish(TOPICO_PUBLISH_1, "s|on");

}

void loop(){
  VerificaConexoesWiFIEMQTT();
  EnviaEstadoOutputMQTT();
  MQTT.loop();

  tempoAtual = millis();

  sensorLuminosidade();
  sensorTempUmid();

  atualizarLCD();

  if (tempoAtual - ultimoUpdateRapido >= intervaloRapido) {
    atualizarLCD();
    ultimoUpdateRapido = tempoAtual;
  }

  if (tempoAtual - ultimaLeituraSensor >= intervaloLeitura) {
    ultimaLeituraSensor = tempoAtual;

    somaTemp = 0;
    somaUmid = 0;

    for (int i = 0; i < leituras; i++) {
      sensorTempUmid();
      sensorLuminosidade();
      somaTemp += temp;
      somaUmid += umid;
    }

    mediaTemp = somaTemp / leituras;
    mediaUmid = somaUmid / leituras;

    alarmeLuminosidade = false;
    alarmeTemperatura = false;
    alarmeUmidade = false;
    condicaoNormal = true;

    if (lux <= meia_luz) {
      digitalWrite(LED_VERD, HIGH);
      digitalWrite(LED_AM, LOW);
      digitalWrite(LED_VERM, LOW);
    } 
    else if (lux > meia_luz && lux < muito_claro) {
      digitalWrite(LED_VERD, LOW);
      digitalWrite(LED_AM, HIGH);
      digitalWrite(LED_VERM, LOW);
      mostrarMensagem("Ambiente", "meia luz", PRIORIDADE_MEDIA);
      condicaoNormal = false;
    } 
    else if (lux >= muito_claro) {
      digitalWrite(LED_VERD, LOW);
      digitalWrite(LED_AM, LOW);
      digitalWrite(LED_VERM, HIGH);
      mostrarMensagem("Ambiente", "muito claro", PRIORIDADE_ALTA);
      alarmeLuminosidade = true;
      condicaoNormal = false;
    }

    if(temp > 15){
      digitalWrite(LED_VERD, LOW);
      digitalWrite(LED_AM, HIGH);
      digitalWrite(LED_VERM, LOW);
      mostrarMensagem("Temp Alta", String(temp, 1) + "C", PRIORIDADE_ALTA);
      alarmeTemperatura = true;
      condicaoNormal = false;
    } 
    else if (temp < 10){
      digitalWrite(LED_VERD, LOW);
      digitalWrite(LED_AM, HIGH);
      digitalWrite(LED_VERM, LOW);
      mostrarMensagem("Temp Baixa", String(temp, 1) + "C", PRIORIDADE_ALTA);
      alarmeTemperatura = true;
      condicaoNormal = false;
    }

    if(umid < 50){
      digitalWrite(LED_VERD, LOW);
      digitalWrite(LED_AM, LOW);
      digitalWrite(LED_VERM, HIGH);
      mostrarMensagem("Umidade Baixa", String(umid, 1) + "%", PRIORIDADE_ALTA);
      alarmeUmidade = true;
      condicaoNormal = false;
    }    
    else if(umid > 70){
      digitalWrite(LED_VERD, LOW);
      digitalWrite(LED_AM, LOW);
      digitalWrite(LED_VERM, HIGH);
      
      mostrarMensagem("Umidade Alta", String(umid, 1) + "%", PRIORIDADE_ALTA);
      alarmeUmidade = true;
      condicaoNormal = false;
    }

    if (condicaoNormal){
      mostrarMensagem("Temp: " + String(mediaTemp,1) + "C","Umid: " + String(mediaUmid,1) + "%", PRIORIDADE_NORMAL);
      prioridadeAtual = PRIORIDADE_NORMAL;

      lcd.setCursor(14, 0); 
      lcd.write(byte(0));
      lcd.setCursor(15, 0); 
      lcd.write(byte(1));
      lcd.setCursor(14, 1); 
      lcd.write(byte(2));
      lcd.setCursor(15, 1); 
      lcd.write(byte(3));
    }

  if (!controleManual) {
    if (alarmeLuminosidade || alarmeTemperatura || alarmeUmidade) {
      ligarBuzz();
    } else {
      desligarBuzz();
    }
  }

  AtualizaEstadoSaida();

  if (controleManual && millis() - tempoUltimoComando > TEMPO_RESET_MANUAL) {
    controleManual = false;
    Serial.println("Modo automático reativado após tempo limite");
  }
    
  }

}
