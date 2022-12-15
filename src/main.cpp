#include <Arduino.h>
#include <String.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

float decodePayload(String Payload, char operation);
float axisToDegress(float Axis_x, float Axis_y,float Axis_z, char operatation);

char readaddress [30];
String addressRef = "ac:23:3f:a9:e7:fb";
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
        float Axis_X = decodePayload(payloadStr,'x');
        float Axis_Y = decodePayload(payloadStr,'y');
        float Axis_Z = decodePayload(payloadStr,'z');

        float A_X = axisToDegress(Axis_X, Axis_Y,Axis_Z, 'x');
        float A_Y = axisToDegress(Axis_X, Axis_Y,Axis_Z, 'y');
        float A_Z = axisToDegress(Axis_X, Axis_Y,Axis_Z, 'z');

        Serial.print("X: ");
        Serial.print(Axis_X);
        Serial.print(" Y: ");
        Serial.print(Axis_Y);
        Serial.print(" Z: ");
        Serial.println(Axis_Z);


        Serial.print("Angles θ: ");
        Serial.print(A_X);
        Serial.print(" Ψ: ");
        Serial.print(A_Y);
        Serial.print(" φ: ");
        Serial.println(A_Z);
        Serial.println(" ");

      }
    }
};




void buscar()
{
    BLEScanResults foundDevices;
    foundDevices = BLEDevice::getScan()->start(2);
    pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
}


void setup() 
{

  Serial.begin(9600);
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449); // less or equal setInterval value

}

void loop() 
{  
  delay(1000);
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


