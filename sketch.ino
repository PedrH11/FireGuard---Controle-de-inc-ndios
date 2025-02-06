#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h> // Para a função log

#define I2C_ADDR    0x27
#define LCD_COLUMNS 20
#define LCD_LINES   4

// Definição dos pinos
#define btn_on 25         // Botão ativa/desativa
#define BI 33            // Botão incremento
#define BD 14             // Botão decremento
#define temp_sensor 35    // Sensor NTC no pino GPIO35
#define gas_sensor 34    // Potenciômetro simulando sensor de gás
#define sprinkler 27      // Sprinkler
#define alarm_son 18      // Alarme sonoro
#define alarm_led 5       // LED de alerta

const char* ssid = "Wokwi-GUEST";
const char* key = "";
const char* broker = "test.mosquitto.org";
int port = 1883;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_LINES);

// Configurações do NTC
const float R_FIXED = 10000.0;        // Valor do resistor fixo no divisor (10 kΩ)
const float BETA = 3950;              // Constante Beta do NTC
const float TEMP_REF = 298.15;        // Temperatura de referência em Kelvin (25°C)
const float R0 = 10000.0;             // Resistência do NTC a 25°C (10 kΩ)

enum EstadoPrincipal{
  Desativado = 0, Ativado = 1, AjusteTPD = 2, increTPD = 3, decreTPD = 4
}estadoPrincipal;

enum MonitoraCalor{
  MonitorandoC =0, PreDisparoC =1, DetectadoCalor=2
}monitoraCalor;

enum MonitoraGas{
  MonitorandoG =0, PreDisparoG =1, DetectadoGas=2
}monitoraGas;

int tIni, tFin;
int TPD=10, GL=350;
float TL=57;
unsigned long tempoPreDisparoInicioG = 0;
unsigned long tempoPreDisparoInicioC = 0;

byte estadoPrincipalAlterado;
byte estadoMonitoraGasAlterado;
byte estadoMonitoraCalorAlterado;

void conexaoWiFi() {
  //Conexão ao Wi-Fi
  Serial.print("Conectando-se ao Wi-Fi ");  
  Serial.print(ssid);
  Serial.print(" ");
  WiFi.begin(ssid, key, 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" Conectado!");
}

void conexaoBroker() {
  //Conexão ao Broker
  mqttClient.setServer(broker, port);
  while (!mqttClient.connected()) {
    Serial.print("Conectando-se ao broker ");    
    //if (mqttClient.connect(WiFi.macAddress().c_str())) {
    if (mqttClient.connect("87987ji0938j1289KJSAUE3")) {
      Serial.println(" Conectado!");            
    } else {
      Serial.print(".");      
      delay(100);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Configuração dos pinos
  pinMode(btn_on, INPUT_PULLDOWN);
  pinMode(BI, INPUT_PULLDOWN);
  pinMode(BD, INPUT_PULLDOWN);
  pinMode(sprinkler, OUTPUT);
  pinMode(alarm_son, OUTPUT);
  pinMode(alarm_led, OUTPUT);

  estadoPrincipal = Desativado;
  estadoPrincipalAlterado = 1;
  
  monitoraCalor =MonitorandoC;
  estadoMonitoraCalorAlterado =1;

  monitoraGas =MonitorandoG;
  estadoMonitoraGasAlterado = 1;

  lcd.init();    // Inicializa o LCD
  lcd.backlight();  // Ativa o backlight do LCD
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");
  lcd.clear();

  conexaoWiFi();
  conexaoBroker();  
}

void processamentoDesativado(){
  monitoraCalor = MonitorandoC;
  monitoraGas = MonitorandoG;

  if (!digitalRead(btn_on)){

    if (tFin < tIni) {
      // Corrige caso tFin seja menor que tIni (erro de reinicialização)
      tIni = millis();
      tFin = tIni;
    }

    if (((tFin-tIni)>0) && ((tFin-tIni) < 3000)) {
      estadoPrincipal = Ativado;
      estadoPrincipalAlterado = 1;
      lcd.clear();
    }
    if ((tFin-tIni) >= 3000) {
      estadoPrincipal = AjusteTPD; 
      estadoPrincipalAlterado = 1;
      lcd.clear();
    }

    tIni = millis();
    tFin = tIni;

  } else {
    tFin = millis();
  }

  digitalWrite(alarm_son, LOW);
  digitalWrite(sprinkler, LOW);
  digitalWrite(alarm_led, LOW);

  lcd.setCursor(0, 0);
  lcd.print("Sistema Desativado"); 

  Serial.println("Estado Desativado");
  Serial.println(tFin-tIni);
}

void processamentoAtivado(){
  if (digitalRead(btn_on)) {
    // Aguarda o botão ser solto antes de mudar o estado
    while (digitalRead(btn_on)) {
      delay(10);
    }

    estadoPrincipal = Desativado;
    estadoPrincipalAlterado = 1;
    tIni = millis();
    tFin = tIni;
    lcd.clear();
  }

  digitalWrite(alarm_son, LOW);
  digitalWrite(sprinkler, LOW);
  digitalWrite(alarm_led, LOW);

  lcd.setCursor(0, 0); 
  lcd.print("Sistema Ativado"); 

  Serial.println("Estado Ativado");
}

void processamentoAjusteTPD(){
    tFin = millis();
    if ((tFin-tIni)>10000) {
      estadoPrincipal = Desativado;
      estadoPrincipalAlterado = 1;
      tIni = millis(); 
      lcd.clear();
    }
  
  if(digitalRead(BI)){
    delay(300);
    estadoPrincipal = increTPD;
    estadoPrincipalAlterado = 1;
  }

  if(digitalRead(BD)){
    delay(300);
    estadoPrincipal = decreTPD;
    estadoPrincipalAlterado = 1;
  }

  digitalWrite(alarm_son, LOW);
  digitalWrite(sprinkler, LOW);
  digitalWrite(alarm_led, LOW);

  lcd.setCursor(0, 0);
  lcd.print("Ajustando Tempo"); 
  Serial.println("Estado AjusteTPD");
  Serial.println(TPD);
  Serial.println(tFin-tIni);
}

void processamentoincreTPD(){
  if(TPD<20){
    TPD++;
  }
  
  estadoPrincipal = AjusteTPD;
  estadoPrincipalAlterado = 1;
  
  digitalWrite(alarm_son, LOW);
  digitalWrite(sprinkler, LOW);
  digitalWrite(alarm_led, LOW);

  lcd.setCursor(0, 0); 
  lcd.print("Atualizado Tempo: ");
  lcd.print(TPD); 

  Serial.println("Estado increTPD");
}

void processamentodecreTPD(){
   if(TPD>5){
    TPD--;
  }

  estadoPrincipal = AjusteTPD;
  estadoPrincipalAlterado = 1;

  digitalWrite(alarm_son, LOW);
  digitalWrite(sprinkler, LOW);
  digitalWrite(alarm_led, LOW);

  lcd.setCursor(0, 0); 
  lcd.print("Atualizado Tempo: ");
  lcd.print(TPD); 

  Serial.println("Estado decreTPD");
}

void processamentoMonitorandoC(){
  int temp_raw = analogRead(temp_sensor);
  float voltage = temp_raw * (3.3 / 4095.0); // Converte ADC para tensão
  float r_ntc = (R_FIXED * voltage) / (3.3 - voltage); // Calcula resistência do NTC
  float temp_k = 1.0 / (1.0 / TEMP_REF + (1.0 / BETA) * log(r_ntc / R0)); // Temp em Kelvin
  float temp_c = temp_k - 273.15; // Converte para Celsius

  Serial.println("Estado MonitorandoC");

  if(temp_c >TL){
    delay(300);
    monitoraCalor = PreDisparoC;
    estadoMonitoraCalorAlterado =1;
    lcd.clear();
  }

  digitalWrite(alarm_son, LOW);
  digitalWrite(sprinkler, LOW);
  digitalWrite(alarm_led, LOW);

  lcd.setCursor(0, 1);
  lcd.print("Temperatura(C):");
  lcd.print(temp_c);
}

void processamentoPredisparoC(){
  int temp_raw = analogRead(temp_sensor);
  float voltage = temp_raw * (3.3 / 4095.0); // Converte ADC para tensão
  float r_ntc = (R_FIXED * voltage) / (3.3 - voltage); // Calcula resistência do NTC
  float temp_k = 1.0 / (1.0 / TEMP_REF + (1.0 / BETA) * log(r_ntc / R0)); // Temp em Kelvin
  float temp_c = temp_k - 273.15; // Converte para Celsius

  if (monitoraCalor == PreDisparoC && tempoPreDisparoInicioC == 0) {
    tempoPreDisparoInicioC = millis(); // Registra o momento de início do pré-disparo
  }

  if(temp_c < TL){
    monitoraCalor = MonitorandoC;
    estadoMonitoraCalorAlterado =1;
    tempoPreDisparoInicioC = 0;
  }else{
    unsigned long tempoAtual = millis();
    if ((tempoAtual - tempoPreDisparoInicioC) >= (TPD * 1000)) {
      monitoraCalor = DetectadoCalor;
      estadoMonitoraCalorAlterado =1; // Transição para o estado de calor detectado
      tempoPreDisparoInicioC = 0;      // Reinicia o controle do tempo
    }
    Serial.println(tempoAtual-tempoPreDisparoInicioC);
  }

  digitalWrite(alarm_son, LOW);
  digitalWrite(sprinkler, LOW);
  digitalWrite(alarm_led, LOW);

  lcd.setCursor(0, 1);
  lcd.print("Temperatura Crítica ");
  Serial.println("Estado PredisparoC");
}

void processamentoDetectadoCalor(){
  digitalWrite(alarm_son, HIGH);
  digitalWrite(sprinkler, HIGH);
  digitalWrite(alarm_led, HIGH);

  tone(18, 1500, 200); // Frequência alta (1500 Hz) por 200 ms
  tone(18, 800, 200);  // Frequência baixa (800 Hz) por 200 ms

  lcd.setCursor(0,1);
  lcd.print("INCÊNDIO IMINENTE");
  Serial.println("Estado DetectadoCalor");
}

void processamentoMonitorandoG(){
  int gas_raw = analogRead(gas_sensor);      
  int gas_ppm = map(gas_raw, 0, 4095, 0, 500);

  if(gas_ppm > GL){
    monitoraGas = PreDisparoG;
    estadoMonitoraGasAlterado =1;
  }

  lcd.setCursor(0, 2);
  lcd.print("Gás(PPM): ");
  lcd.print(gas_ppm);

  Serial.println("Estado MonitorandoG");
}

void processamentoPreDisparoG(){
  int gas_raw = analogRead(gas_sensor);      
  int gas_ppm;
  gas_ppm = map(gas_raw, 0, 4095, 0, 500);

  if (monitoraGas == PreDisparoG && tempoPreDisparoInicioG == 0) {
    tempoPreDisparoInicioG = millis(); // Registra o momento de início do pré-disparo
  }

  if(gas_ppm < GL){
    monitoraGas = MonitorandoG;
    estadoMonitoraGasAlterado =1;
    tempoPreDisparoInicioG = 0;
  }else{
    unsigned long tempoAtual = millis();
    if ((tempoAtual - tempoPreDisparoInicioG) >= (TPD * 1000)) {
      monitoraGas = DetectadoGas; // Transição para o estado de calor detectado
      estadoMonitoraGasAlterado =1;
      tempoPreDisparoInicioG = 0;      // Reinicia o controle do tempo
    }
    Serial.println(tempoAtual - tempoPreDisparoInicioG);
  }

  digitalWrite(alarm_son, LOW);
  digitalWrite(sprinkler, LOW);
  digitalWrite(alarm_led, LOW);
  
  lcd.setCursor(0, 2);
  lcd.print("CONCENTRACAO CRITICA");
  Serial.println("Estado PreDisparoG");
}

void processamentoDetectadoGas(){
  digitalWrite(alarm_son, HIGH);
  digitalWrite(sprinkler, LOW);
  digitalWrite(alarm_led, HIGH);

  tone(18, 1500, 200); // Frequência alta (1500 Hz) por 200 ms
  tone(18, 800, 200);  // Frequência baixa (800 Hz) por 200 ms 
  
  lcd.setCursor(0, 2);
  lcd.print("INCÊNDIO IMINENTE");
  
  Serial.println("Estado DetectadoGas");
}

void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconexão ao Wi-Fi...");
    conexaoWiFi();
  }
  if (!mqttClient.connected()) {
    Serial.println("Reconexão ao broker...");
    conexaoBroker();
  }

   switch(estadoPrincipal) {
    case Desativado:
      if(estadoPrincipalAlterado){
        estadoPrincipalAlterado=0;
        mqttClient.publish("EstadoPrincipal", "Desativado");
      }
      processamentoDesativado();
      break;
    case Ativado:
      if(estadoPrincipalAlterado){
        lcd.clear();
        estadoPrincipalAlterado=0;
        mqttClient.publish("EstadoPrincipal", "Ativado");
      }
      processamentoAtivado();
      break;
    case AjusteTPD:
      if(estadoPrincipalAlterado){
        estadoPrincipalAlterado=0;
        mqttClient.publish("EstadoPrincipal", "AjusteTPD");
      }
      processamentoAjusteTPD();
      break;
    case increTPD:
      if(estadoPrincipalAlterado){
        estadoPrincipalAlterado=0;
        mqttClient.publish("EstadoPrincipal", "IncreTPD");
      }
      processamentoincreTPD();
      break;  
     case decreTPD:
      if(estadoPrincipalAlterado){
        estadoPrincipalAlterado=0;
        mqttClient.publish("EstadoPrincipal", "DecreTPD");
      }
      processamentodecreTPD();
      break; 
  } 

  if(estadoPrincipal==Ativado){
    switch(monitoraCalor) {
    case MonitorandoC:
      if(estadoMonitoraCalorAlterado){
        estadoMonitoraCalorAlterado=0;
        mqttClient.publish("EstadoMonitoraCalor", "MonitorandoC");
      }
      processamentoMonitorandoC();
      break;
    case PreDisparoC:
      if(estadoMonitoraCalorAlterado){
        estadoMonitoraCalorAlterado=0;
        mqttClient.publish("EstadoMonitoraCalor", "PredisparoC");
      }
      processamentoPredisparoC();
      break;
    case DetectadoCalor:
      if(estadoMonitoraCalorAlterado){
        estadoMonitoraCalorAlterado=0;
        mqttClient.publish("EstadoMonitoraCalor", "DetectadoCalor");
      }
      processamentoDetectadoCalor();
      break;
    } 
  }

  if(estadoPrincipal==Ativado){
    switch(monitoraGas) {
    case MonitorandoG:
      if(estadoMonitoraGasAlterado){
        lcd.clear();
        estadoMonitoraGasAlterado=0;
        mqttClient.publish("EstadoMonitoraGas", "MonitorandoG");
      }
      processamentoMonitorandoG();
      break;
    case PreDisparoG:
      if(estadoMonitoraGasAlterado){
       estadoMonitoraGasAlterado=0;
       mqttClient.publish("EstadoMonitoraGas", "PredisparoG");
     }
      processamentoPreDisparoG();
      break;
    case DetectadoGas:
      if(estadoMonitoraGasAlterado){
        estadoMonitoraGasAlterado=0;
       mqttClient.publish("EstadoMonitoraGas", "DetectadoGas");
     }
      processamentoDetectadoGas();
      break;
    } 
  }
  delay(50);
} 
