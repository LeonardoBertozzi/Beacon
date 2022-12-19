#include <Arduino.h>
#include <String.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

float decodePayload(String Payload, char operation);
float axisToDegress(float Axis_x, float Axis_y,float Axis_z, char operatation);
void mountPackage();
void processingData();
void stateGPIO(void * params);

xQueueHandle QueuePackages;
xSemaphoreHandle state; // Semaphore para as Tasks

char estado;


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


char readaddress [30];
//String addressRef = "ac:23:3f:a9:e7:fb";
String addressRef = "ac:23:3f:aa:82:29";
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

      Address01 = advertisedDevice.getAddress().toString().c_str();
      
      if(strcmp(Address01.c_str(), addressRef.c_str()) == 0)
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


void stateGPIO(void * params)
{
  while(1)
  {    
    switch (estado)
    {

      case 'A':
        digitalWrite(13,LOW);
        Serial.println("Bag armado");
        break;

      case 'D':
        digitalWrite(13,HIGH);
        Serial.println("Bag Desarmado");
        break;

      case 'N':
        digitalWrite(13,HIGH);
        break;
    }

    vTaskDelay(2000/ portTICK_RATE_MS);
  }
}


void mountPackage()
{
    String package;
    acc_frame frame;
    
    //package = (package + frame.Axis_x + "," + frame.Axis_y + "," + frame.Axis_z + "," + frame.Angle_x + "," + frame.Angle_y + "," + frame.Angle_z);
    //Serial.printf("Package: %s \r\n",package.c_str()); 


    long resposta = xQueueSend(QueuePackages, &frame, 1000 / portTICK_PERIOD_MS);
    
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
  if(xQueueReceive(QueuePackages, &frame, 5000 / portTICK_PERIOD_MS))
  {

    if ( (frame.Angle_x >= 15 && frame.Angle_x <= 60) && (frame.Angle_y >= 40 && frame.Angle_y <= 70) )
    {
      //Serial.println("Bag armado");
      if (xSemaphoreTake(state, 2000 / portTICK_PERIOD_MS)) // Pega o Semaphore se ele estiver disponivel
      {
        estado = 'A';
        xSemaphoreGive(state); // Devolve o Semaphore após terminar a função
      
      }
    }

    else if ( (frame.Angle_x >= 70 && frame.Angle_x <= 90) && (frame.Angle_y < 3 ) )
    {
      //Serial.println("Bag desarmado");
      if (xSemaphoreTake(state, 2000 / portTICK_PERIOD_MS)) // Pega o Semaphore se ele estiver disponivel
      {
        
        estado = 'D';
        xSemaphoreGive(state); // Devolve o Semaphore após terminar a função
      
      }
    }
    else
    {
      if (xSemaphoreTake(state, 2000 / portTICK_PERIOD_MS)) // Pega o Semaphore se ele estiver disponivel
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
    foundDevices = BLEDevice::getScan()->start(1);
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
}


void setup() 
{

  pinMode(13,OUTPUT);
  Serial.begin(9600);
  state = xSemaphoreCreateMutex();
  QueuePackages = xQueueCreate(12,sizeof(float));
  xTaskCreate(&stateGPIO, "estado da porta", 2048, NULL, 1, NULL);

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449); // less or equal setInterval value

}

void loop() 
{
  buscar();
  
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


