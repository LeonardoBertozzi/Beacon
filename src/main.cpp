#include <Arduino.h>
#include <String.h>
#include <stdlib.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <nvs.h>
#include <nvs_flash.h>

float decodePayload(String Payload, char operation);
float axisToDegress(float Axis_x, float Axis_y,float Axis_z, char operatation);
void mountPackage();
void processingData();

String NVS_Read_String(const char* name, const char* key);
int NVS_Write_String(const char* name, const char* key, const char* stringVal);
void NVS_Erase();
void Task_stateGPIO(void * params);
void Task_registerBeacon(void * params);
void registerBeacon();



#define SEMAPHORE_WAIT 2000
#define QUEUE_WAIT 3000
#define QUEUE_LENGHT 12
#define SERIAL_BAUDRATE 9600
#define TIME_SEARCH_BLE 1
#define TIME_REGISTER 10000
#define PIN_ED2 13
#define PIN_ED3 14
#define LED_BLUE 2



xQueueHandle QueuePackages;
xSemaphoreHandle state; // Semaphore para as Tasks

char estado;
char flag = 'I';

struct acc_frame 
{
  String index;
  float Axis_x;
  float Axis_y;
  float Axis_z;
  float Angle_x;
  float Angle_y;
  float Angle_z;
}frame = {"BACC",0,0,0,0,0,0};


//String addressRef = "ac:23:3f:aa:82:29";
String ED2;
String ED3;
String addrMac;




BLEScan* pBLEScan;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {

      size_t payloadLen = advertisedDevice.getPayloadLength();
      uint8_t* payloadData = advertisedDevice.getPayload();
      String payloadStr;
      String Address01;
      
      for (int i = 0; i < payloadLen; i++) 
      {
        char payloadByte[3];
        sprintf(payloadByte, "%02X", payloadData[i]);
        payloadStr = payloadStr + payloadByte;

      }
      //Serial.print(payloadStr);

      Address01 = advertisedDevice.getAddress().toString().c_str();
      
      if(strcmp(Address01.c_str(), ED2.c_str()) == 0)
      {
        frame.Axis_x = decodePayload(payloadStr,'x');
        frame.Axis_y = decodePayload(payloadStr,'y');
        frame.Axis_z = decodePayload(payloadStr,'z');

        frame.Angle_x = axisToDegress(frame.Axis_x,frame.Axis_y,frame.Axis_z,'x');
        frame.Angle_y = axisToDegress(frame.Axis_x,frame.Axis_y,frame.Axis_z,'y');
        frame.Angle_z = axisToDegress(frame.Axis_x,frame.Axis_y,frame.Axis_z,'z');
        mountPackage();
        processingData();

      }
    }
};


void Task_stateGPIO(void * params)
{
  while(1)
  {    
    switch (estado)
    {

      case 'A':
        digitalWrite(PIN_ED2,LOW);
        digitalWrite(LED_BLUE,HIGH);
        Serial.println("Bag armado");
        break;

      case 'D':
        digitalWrite(PIN_ED2,HIGH);
        digitalWrite(LED_BLUE,LOW);
        Serial.println("Bag Desarmado");
        break;

      case 'N':
        digitalWrite(PIN_ED2,HIGH);
        digitalWrite(LED_BLUE,LOW);
        break;
    }

    vTaskDelay(2000 / portTICK_RATE_MS);
  }
}


void Task_registerBeacon(void * params)
  {
    while(1)
    {
      if(millis() < TIME_REGISTER)
      {
        if (Serial.available() > 0) 
        {
          addrMac = Serial.readString();
          Serial.println(addrMac);
          flag = 'G';
        }
      }

      if (flag == 'I' && (millis() > TIME_REGISTER))
      {
        ED2 = NVS_Read_String("memoria", "ED2");
        ED3 = NVS_Read_String("memoria", "ED3");
        Serial.println(" ");
        Serial.printf("iBeacon ED2 -> ");
        Serial.println(ED2);
        Serial.printf("iBeacon ED3 -> ");
        Serial.println(ED3);

        flag = 'N';
      }
      
      if (flag == 'G')
      {
        String str1 = addrMac.substring(0,17);
        String str2 = addrMac.substring(18,36);
        const char* addrMacED2 = str1.c_str();
        const char* addrMacED3 = str2.c_str();
        NVS_Write_String("memoria", "ED2", addrMacED2);
        NVS_Write_String("memoria", "ED3", addrMacED3);
        flag = 'I' ;
      }

      vTaskDelay(1000 / portTICK_RATE_MS);
    }
  }


void mountPackage()
{
    String package;
    acc_frame frame;
    //package = (package + frame.Axis_x + "," + frame.Axis_y + "," + frame.Axis_z + "," + frame.Angle_x + "," + frame.Angle_y + "," + frame.Angle_z);
    //Serial.printf("Package: %s \r\n",package.c_str()); 

    long resposta = xQueueSend(QueuePackages, &frame, QUEUE_WAIT / portTICK_PERIOD_MS);
    
    if(resposta == true)
    {
     //Serial.println("add"); 
    }
    else
    {
      Serial.printf("Não adicionado a fila: %s \r\n",package.c_str());
    }
    
}

void processingData()
{
  if(xQueueReceive(QueuePackages, &frame, QUEUE_WAIT / portTICK_PERIOD_MS))
  {

    if ( (frame.Angle_x >= 15 && frame.Angle_x <= 60) && (frame.Angle_y >= 40 && frame.Angle_y <= 70) )
    {
      //Serial.println("Bag armado");
      if (xSemaphoreTake(state, SEMAPHORE_WAIT / portTICK_PERIOD_MS)) // Pega o Semaphore se ele estiver disponivel
      {
        estado = 'A';
        xSemaphoreGive(state); // Devolve o Semaphore após terminar a função
      }
    }

    else if ( (frame.Angle_x >= 70 && frame.Angle_x <= 90) && (frame.Angle_y < 3 ) )
    {
      //Serial.println("Bag desarmado");
      if (xSemaphoreTake(state, SEMAPHORE_WAIT / portTICK_PERIOD_MS)) // Pega o Semaphore se ele estiver disponivel
      {
        estado = 'D';
        xSemaphoreGive(state); // Devolve o Semaphore após terminar a função
      }
    }
    else
    {
      if (xSemaphoreTake(state, SEMAPHORE_WAIT / portTICK_PERIOD_MS)) // Pega o Semaphore se ele estiver disponivel
      {
        estado = 'N';
        xSemaphoreGive(state); // Devolve o Semaphore após terminar a função
      }
    }
  }
}


void buscar()
{
    BLEScanResults foundDevices;
    foundDevices = BLEDevice::getScan()->start(TIME_SEARCH_BLE);
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
}


void setup() 
{
  Serial.begin(SERIAL_BAUDRATE);
  pinMode(13,OUTPUT);
  pinMode(2,OUTPUT);
  Serial.println("");
  Serial.println("Digite o endereço MAC dos iBeacons referentes a ED2 e ED3 sepados por '@'.");
  state = xSemaphoreCreateMutex();
  QueuePackages = xQueueCreate(QUEUE_LENGHT,sizeof(float));
  xTaskCreate(&Task_stateGPIO, "estado da porta", 2048, NULL, 1, NULL);
  xTaskCreate(&Task_registerBeacon, "Registrar Beacon", 2048, NULL, 2, NULL);

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449); // less or equal setInterval value

}


float decodePayload(String Payload, char operation)
{

  String Payload1;
  String Payload2;
  float axis;
  float x;

  switch (operation)
  {
    case 'x':
      // Separando duas partes do payloaX //
      Payload1 = Payload.substring(28,30);
      Payload2 = Payload.substring(30,32);
      break;

    case 'y':
      // Separando duas partes do payloaY //
      Payload1 = Payload.substring(32,34);
      Payload2 = Payload.substring(34,36);
      break;

    case 'z':
      // Separando duas partes do payloaZ //
      Payload1 = Payload.substring(36,38);
      Payload2 = Payload.substring(38,40);
      break;

    default:
      break;
  }

  if (Payload1 == "FF")
  {
    x = -1;
  }
  else if (Payload1 == "00")
  {
    x = 0;
  }
  else
  {
    x = 1;
  }

  int n = Payload2.length();
  char char_array [n+1];
  strcpy(char_array,Payload2.c_str());
  axis = strtol(char_array,NULL,16);  //Conversão de base hexa to dec
  axis = (axis/256) + x;
  return axis;

}


float axisToDegress(float Axis_x, float Axis_y,float Axis_z, char operatation)
{
  float angle = 0;

  switch (operatation)
  {
    case 'x':
      angle = atan((Axis_x)/(sqrt(pow(Axis_y,2) + pow(Axis_z,2))));
      break;

    case 'y':
      angle = atan((Axis_y)/(sqrt(pow(Axis_x,2) + pow(Axis_z,2))));
      break;
    
    case 'z':
      angle = atan((sqrt(pow(Axis_x,2) + pow(Axis_y,2)))/(Axis_z));
      break;

    default:
      break;
  }
  
  angle = (angle*180)/3.14;
  return angle;

}



void NVS_Erase()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    esp_err_t retorno = nvs_flash_erase();
    if ((retorno =! ESP_OK))
    {
      Serial.printf("Não apagou nada : (%s) \n\r", esp_err_to_name(retorno));
    }
}


int NVS_Write_String(const char* name, const char* key, const char* stringVal)
{
    nvs_handle particao_handle;
    esp_err_t retVal;
    int aux;
    
    ESP_ERROR_CHECK(nvs_flash_init());
    retVal = nvs_open(name, NVS_READWRITE, &particao_handle);
    if(retVal != ESP_OK)
    {
        Serial.println("Não foi possivel acessar a partição");
        aux = 0;
    }
    else
    {
        //Serial.println("opening NVS Write handle Done");
        retVal = nvs_set_str(particao_handle, key, stringVal);

        if(retVal != ESP_OK)
        {
            Serial.println("Não foi possivel  gravar o dado enviado");
            aux = 0;
        }
 
        retVal = nvs_commit(particao_handle);
        if(retVal != ESP_OK)
        {
            Serial.println("Não foi possivel  gravar o dado enviado na partição ");
            aux = 0;
        }
        else
        {
            //Serial.println(" Dado Gravado na memoria com sucesso");
            aux = 1;
        }
       
    }
 
    nvs_close(particao_handle);
    return aux;

}


String NVS_Read_String(const char* name, const char* key)
{
    nvs_handle particao_handle;
    size_t required_size;
    ESP_ERROR_CHECK(nvs_flash_init());
    esp_err_t res_nvs = nvs_open(name, NVS_READONLY, &particao_handle);

    if (res_nvs == ESP_ERR_NVS_NOT_FOUND)
    {
      Serial.printf("NVS, Namespace: armazenamento, não encontrado =/");
    }
    else
    {
      nvs_get_str(particao_handle, key, NULL , &required_size);
      char* data = (char*)malloc(required_size);

      esp_err_t res = nvs_get_str(particao_handle, key, data, &required_size);
      switch (res)
      {
      case ESP_OK:
        //Serial.printf("Valor Armazenado: %s ",data);
        break;

      case ESP_ERR_NOT_FOUND:
        Serial.printf("NVS: Valor não encontrado");
        break;

      default:
        Serial.printf("NVS: Erro ao acessar o NVS (%s)", esp_err_to_name(res));
        break;
      }
      nvs_close(particao_handle);
      return data;
    }
    return "";
}


//ac:23:3f:aa:82:29
void loop() 
{

  buscar();

}

