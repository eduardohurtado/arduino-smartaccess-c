#include <SPI.h>      // incluye libreria bus SPI
#include <MFRC522.h>      // incluye libreria especifica para MFRC522
#include <Ethernet.h> //libreria ethernet
#include <EEPROM.h>
#include <Keypad.h>
#include <SoftwareSerial.h>
#include "FPM.h"
#include <LiquidCrystal_I2C.h> // Libreria LCD_I2C
//#include <Wire.h>   // incluye libreria para interfaz I2C

#define SS_PIN  53        // constante para referenciar pin de slave select
#define RST_PIN  49       // constante para referenciar pin de reset
#define buzzer    38
#define led_rojo  39
#define rele      36

// Configuracion del Ethernet Shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = {  192,168,1,9 };
char sever[] = "192.168.1.100"; // IP Adres (or name) of server to dump data to

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

String operacion= "";             //Variable para enviar el dato de usuario al servidor
String str= "";                     
String seguridad= "";
int contadorLan= 0;

SoftwareSerial fserial(A8, A9);     //Comunicacion serial sensor huella

FPM finger(&fserial);
FPM_System_Params params;

MFRC522 mfrc522(SS_PIN, RST_PIN);    // crea objeto mfrc522 enviando pines de slave select y reset

LiquidCrystal_I2C lcd(0x27,16,2);   //Crea el Display LCD

int16_t fid;                        //Variable para ingresar una posicion de huella

byte LecturaUID[4]= {0xFF, 0xFF, 0xFF, 0xFF} ;        // crea array para almacenar el UID leido
byte Eliminar[4]= {0xFF, 0xFF, 0xFF, 0xFF} ;          //Array para la eliminacion de usuarios ya registrados
byte contadorEEPROM= 0;                               //Contador para buscar un UID en la EEPROM

boolean accesoRFID= false;                            //Permitir/Denegar el acceso RFID
boolean accesoHuella= false;
boolean registroRFID= false;                          //Saber si se realizo el registro o no
boolean errorLectura= false;
boolean modoAdmin= false;                             //Comprueba el modo Administrador
boolean displayHome= false;
boolean displayRfid= false;
boolean nuevaHuella= false;
boolean registroH= false;
boolean resetHuella= true;                            //Variable para controlar el acceso con huella

const byte rfidAdmin[4]= {0x1B, 0x20, 0x02, 0x1B} ;   //IDENTIFICADOR DE ADMINISTRADOR***
const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns

const byte eliminarEEPROM= 0xFF;                      //Constante para eliminar toda la memoria EEPROM

unsigned long previousTime= 0;
const long eventTime= 7200000;                         //Tiempo de reinicio de la huella


int error= 0;                                         //Variable para describir diferentes tipos de errores
int numIden= 0;                                    //Variable para controlar el error de identificacion
int numHuella= 0;                                    //Numero asociado al usuario RFID y su huella (1 - 120)
uint8_t numHuella2;
char customKey;
int posHuella[3]= {0, 0, 0};                                    // almacena en un array 3 digitos ingresados
int indice= 0;                                       // Almacena las 3 posiciones para el numero de posicion de la nueva huella
int someInt= 0;
boolean x= false;                                   //Variable para el control de ciclos WHILE

int numA= 0;
int numB= 0;
int numC= 0;

const int contrasenaAdmin= 18011993;        //CONTRASENA DE ADMINISTRADOR PARA ELIMINAR TODOS LOS DATOS DEL SISTEMA
String claveAdmin= "";                          //Variable para almacenar la contrasena ingresada
int contadorClave= 0;
int comprobarClave= 0;
boolean contrasenaBien= false;
boolean displayLimpiar = false;
boolean verificarClave = false;


//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {40, 41, 42, 43}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {44, 45, 46, 47}; //connect to the column pinouts of the keypad

//initialize an instance of class NewKeypad
Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 


void setup() {

  Serial.begin(9600);     // inicializa comunicacion por monitor serie a 9600 bps

  //Wire.begin();

  lcd.init();
  lcd.backlight();

  //Letrero de inicio
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Iniciando");
  lcd.setCursor(0,1);
  lcd.print("Programa...");
  lcd.display();

  Ethernet.begin(mac, ip);
    
  delay(3000);

  pinMode(rele, OUTPUT);              //Relevador
  digitalWrite(rele, HIGH);


  SPI.begin();                    // inicializa bus SPI
  mfrc522.PCD_Init();             // inicializa modulo lector
  Serial.println("Listo");        // Muestra texto Listo

  fserial.begin(57600);             // set the data rate for the sensor serial port

  if (finger.begin()) {
        finger.readParams(&params);
        Serial.println("Found fingerprint sensor!");
        Serial.print("Capacity: "); Serial.println(params.capacity);
        Serial.print("Packet length: "); Serial.println(FPM::packet_lengths[params.packet_len]);

        finger.led_off();
    } else {
        Serial.println("Did not find fingerprint sensor :(");
        digitalWrite(buzzer, HIGH);
        delay(10000);
        digitalWrite(buzzer, LOW);
    }

  pinMode(led_rojo, OUTPUT);            //LED error
  digitalWrite(led_rojo, LOW);
  pinMode(buzzer, OUTPUT);              //Buzzer
  digitalWrite(buzzer, LOW);


  digitalWrite(buzzer, HIGH);           //Pitido de inicio del sistema
  delay(50);
  digitalWrite(buzzer, LOW);
  delay(50);
  digitalWrite(buzzer, HIGH);
  delay(50);
  digitalWrite(buzzer, LOW);

  resetHuella= true;                    //Control de huella para cuando se apaga el sistema

  displayHome= true;
}


void loop() {

  if(displayHome == true){

    lcd.clear();
    lcd.setCursor(5,0);
    lcd.print("UNIAJC");
    lcd.setCursor(0,1);
    lcd.print("Control Ingreso"); 
    lcd.display();

    displayHome= false;
  }

  if(millis() < previousTime){

    previousTime= millis();
  }

  //Reset de huella cada dos minutos
  if(millis() - previousTime >= eventTime){

    digitalWrite(buzzer, HIGH);       //DOBLE PITIDO
    delay(50);
    digitalWrite(buzzer, LOW);
    delay(50);
    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);
      
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("***SEGURIDAD***");
    lcd.setCursor(0,1);
    lcd.print("Reinicio Huella"); 
    lcd.display();
    delay(1000);

    resetHuella= true;
    previousTime= millis();

    displayHome= true;
  }
  
  customKey = customKeypad.getKey();

  if (customKey == 'B'){
    
    lcd.clear();
    lcd.setCursor(3,0);
    lcd.print("MODO ADMIN");
    lcd.setCursor(0,1);
    lcd.print(":Nuevo Registro:"); 
    lcd.display();
    delay(1000);

    modoAdministrador();                 //Compruebo permiso de administrador

    if(modoAdmin == true){

      digitalWrite(buzzer, HIGH);       //DOBLE PITIDO
      delay(50);
      digitalWrite(buzzer, LOW);
      delay(50);
      digitalWrite(buzzer, HIGH);
      delay(50);
      digitalWrite(buzzer, LOW);
      
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ACCESO CONCEDIDO");
      lcd.setCursor(1,1);
      lcd.print("ADMINISTRADOR"); 
      lcd.display();
      delay(3000);
    
      registrarUID();
      displayHome= true;
      modoAdmin= false;
    }else{

      lcd.clear();
      lcd.setCursor(4,0);
      lcd.print("*ERROR*");
      lcd.setCursor(0,1);
      lcd.print("Usuario Denegado"); 
      lcd.display();

      digitalWrite(buzzer, HIGH);
      delay(1000);
      digitalWrite(buzzer, LOW);
      delay(3000);

      displayHome= true;
    }

  }else if(customKey == 'A'){

    identificarUID();
      
  }else if(customKey == 'C'){

    eliminarUsuario();
  }else if(customKey == 'D'){

    limpiarTodo();
  }

  customKey = '@';                   
}


void modoAdministrador(){

  displayRfid= true;

  while(errorLectura == false){

    if(displayRfid == true){

      lcd.clear();
      lcd.setCursor(3,0);
      lcd.print("MODO ADMIN");
      lcd.setCursor(0,1);
      lcd.print("Identificar RFID"); 
      lcd.display();

      displayRfid= false;
    }

    digitalWrite(led_rojo, HIGH);

    

    if (mfrc522.PICC_IsNewCardPresent()){   // si hay una tarjeta presente
    if (mfrc522.PICC_ReadCardSerial()){ // si puede obtener datos de la tarjeta

        Serial.print("Tarjeta UID:");       // muestra texto UID:
        for (byte i = 0; i < 4; i++) { // bucle recorre de a un byte por vez el UID
      if (mfrc522.uid.uidByte[i] < 0x10){   // si el byte leido es menor a 0x10
        Serial.print(" 0");       // imprime espacio en blanco y numero cero
        }
        else{           // sino
          Serial.print(" ");        // imprime un espacio en blanco
          }
          Serial.print(mfrc522.uid.uidByte[i], HEX);    // imprime el byte del UID leido en hexadecimal
          LecturaUID[i]=mfrc522.uid.uidByte[i];     // almacena en array el byte del UID leido      
          }
          
          Serial.println("");         // imprime un espacio
          errorLectura= true;      
      } }
  }

  digitalWrite(led_rojo, LOW);
  mfrc522.PICC_HaltA();                    // detiene comunicacion con tarjeta

  if(comparaUID(LecturaUID, rfidAdmin))    // llama a funcion comparaUID con rfidAdmin
            modoAdmin= true;               // si retorna verdadero muestra texto bienvenida
           else                           // si retorna falso
            modoAdmin= false;             // muestra texto equivalente a acceso denegado

  
  errorLectura= false;
}


void registrarUID(){

  displayRfid= true;

  while(errorLectura == false){

    if(displayRfid == true){

      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("Favor Colocar");
      lcd.setCursor(1,1);
      lcd.print("Nueva Tarjeta"); 
      lcd.display();

      displayRfid= false;
    }

    digitalWrite(led_rojo, HIGH);
    
  if (mfrc522.PICC_IsNewCardPresent()){   // si hay una tarjeta presente
    if (mfrc522.PICC_ReadCardSerial()){ // si puede obtener datos de la tarjeta

        Serial.print("Tarjeta UID:");       // muestra texto UID:
        for (byte i = 0; i < 4; i++) { // bucle recorre de a un byte por vez el UID
      if (mfrc522.uid.uidByte[i] < 0x10){   // si el byte leido es menor a 0x10
        Serial.print(" 0");       // imprime espacio en blanco y numero cero
        }
        else{           // sino
          Serial.print(" ");        // imprime un espacio en blanco
          }
          Serial.print(mfrc522.uid.uidByte[i], HEX);    // imprime el byte del UID leido en hexadecimal
          LecturaUID[i]=mfrc522.uid.uidByte[i];     // almacena en array el byte del UID leido      
          }
          
          Serial.println("");         // imprime un espacio
          errorLectura= true;      
      } }  

  }
  
  mfrc522.PICC_HaltA();               // detiene comunicacion con tarjeta
  digitalWrite(led_rojo, LOW);        //Apago el led de lectura

  if(errorLectura== true){

    for (int i = 0; i <= EEPROM.length(); i++){              //Comparar el UID si ya esta registrado en la EEPROM

    
    if(LecturaUID[contadorEEPROM]== EEPROM.read(i)){
      
      contadorEEPROM= contadorEEPROM + 1;
      i= i+1;
      if(LecturaUID[contadorEEPROM]== EEPROM.read(i)){

        contadorEEPROM= contadorEEPROM + 1;
        i= i+1;
        if(LecturaUID[contadorEEPROM]== EEPROM.read(i)){

          contadorEEPROM= contadorEEPROM + 1;
          i= i+1;
          if(LecturaUID[contadorEEPROM]== EEPROM.read(i)){
            
            error = 1;                                      //***ERROR NO.1 el usuario ya esta registrado en sistema
            contadorEEPROM= 0;
            i= EEPROM.length();
            }
          }        
        }      
      }else{ contadorEEPROM= 0;}
    }

    switch(error){

      case 0:

      
        for (int i = 0; i <= EEPROM.length(); i++){    //Buscar memoria disponible en la EEPROM y almacenar el usuario

    
    if(EEPROM.read(i) == 0xFF){
      
      i= i+1;
      if(EEPROM.read(i) == 0xFF){

        i= i+1;
        if(EEPROM.read(i) == 0xFF){

          i= i+1;
          if(EEPROM.read(i) == 0xFF){
            
            EEPROM.put(i-3, LecturaUID);
            registroRFID= true;
            i= EEPROM.length();
            }
          }        
        }      
      }
    }

    if(registroRFID == false){                  //Comprueba si se recorrio toda la memoria y no hallo espacio

    
    lcd.setCursor(4,0);
    lcd.print("*ERROR*");
    lcd.setCursor(1,1);
    lcd.print("Memoria Llena"); 
    lcd.display();
    
    digitalWrite(buzzer, HIGH);                    //Pitido de error de memoria llena
    delay(1000);
    digitalWrite(buzzer, LOW);
    delay(5000);

    lcd.clear(); 
    lcd.display();
    
  }else{

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Registro Tarjeta");
    lcd.setCursor(0,1);
    lcd.print("Ha Sido Exitoso"); 
    lcd.display();

    digitalWrite(buzzer, HIGH);                    //Pitido de usuario registrado
    delay(50);
    digitalWrite(buzzer, LOW);
    delay(50);
    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);
    delay(3000);

    registrarHuella();                             //Registro de huella para el nuevo usuario

    finger.led_off();
  }

  registroRFID= false;
  
        break;

      case 1:
      
        //Serial.println("¡ERROR! codigo #01, El usuario ya se encuentra registrado en el sistema.");
        lcd.clear();
        lcd.setCursor(4,0);
        lcd.print("*ERROR*");
        lcd.setCursor(0,1);
        lcd.print("User Registrado"); 
        lcd.display();
        
        digitalWrite(buzzer, HIGH);                    //Pitido de error de usuario ya registrado
        delay(1000);
        digitalWrite(buzzer, LOW);
        delay(5000);

        lcd.clear(); 
        lcd.display();
        
        error= 0;
        break;
    } 
  }

  errorLectura= false;                  //Comprueba si hubo una tarjeta detectada

}



void identificarUID(){
  
  displayRfid= true;

  while(errorLectura == false){

    digitalWrite(led_rojo, HIGH);

    if(displayRfid == true){

      lcd.clear();
      lcd.setCursor(4,0);
      lcd.print("CONTROL");
      lcd.setCursor(0,1);
      lcd.print("Colocar Tarjeta"); 
      lcd.display();

      displayRfid= false;
    }
    
    
  if (mfrc522.PICC_IsNewCardPresent()){   // si hay una tarjeta presente
    if (mfrc522.PICC_ReadCardSerial()){ // si puede obtener datos de la tarjeta

        Serial.print("Tarjeta UID:");       // muestra texto UID:
    for (byte i = 0; i < 4; i++) { // bucle recorre de a un byte por vez el UID
      if (mfrc522.uid.uidByte[i] < 0x10){   // si el byte leido es menor a 0x10
        Serial.print(" 0");       // imprime espacio en blanco y numero cero
        }
        else{           // sino
          Serial.print(" ");        // imprime un espacio en blanco
          }
          Serial.print(mfrc522.uid.uidByte[i], HEX);    // imprime el byte del UID leido en hexadecimal
          LecturaUID[i]=mfrc522.uid.uidByte[i];     // almacena en array el byte del UID leido      
          }
          
          Serial.println("");         // imprime un espacio
          errorLectura= true;      
      } }  

  }
  mfrc522.PICC_HaltA();               // detiene comunicacion con tarjeta
  errorLectura= false;
  digitalWrite(led_rojo, LOW);        //Apago el led de lectura

  lcd.clear(); 
  lcd.display();
  
  for (int i = 0; i <= EEPROM.length(); i++){                  //Comparar el UID con la informacion en la EEPROM

    
    if(LecturaUID[contadorEEPROM]== EEPROM.read(i)){
      
      contadorEEPROM= contadorEEPROM + 1;
      i= i+1;
      if(LecturaUID[contadorEEPROM]== EEPROM.read(i)){

        contadorEEPROM= contadorEEPROM + 1;
        i= i+1;
        if(LecturaUID[contadorEEPROM]== EEPROM.read(i)){

          contadorEEPROM= contadorEEPROM + 1;
          i= i+1;
          if(LecturaUID[contadorEEPROM]== EEPROM.read(i)){
            
            accesoRFID= true;
            contadorEEPROM= 0;
            i= EEPROM.length();
            }
          }        
        }      
      }else{ contadorEEPROM= 0;}
    }


  if(accesoRFID == true){

    digitalWrite(buzzer, HIGH);          //Doble pitido
    delay(50);
    digitalWrite(buzzer, LOW);
    delay(50);
    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);
    
    lcd.clear();
    lcd.setCursor(4,0);
    lcd.print("Tarjeta");
    lcd.setCursor(3,1);
    lcd.print("Encontrada"); 
    lcd.display();
    delay(500);

    if(resetHuella == true){


      //*************PREGUNTAR POSICION DE HUELLA A VERIFICAR***************

    registroH= true;
    nuevaHuella= true;

    while(registroH == true){                                                   //Leo los tres numeros de posicion

      if(nuevaHuella== true){

        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Ingresar Numero");
        lcd.setCursor(2,1);
        lcd.print("De Identidad"); 
        lcd.display();
        delay(2000);

        digitalWrite(buzzer, HIGH);
        delay(50);
        digitalWrite(buzzer, LOW);
        delay(50);
        digitalWrite(buzzer, HIGH);
        delay(50);
        digitalWrite(buzzer, LOW);
    
        lcd.clear();
        lcd.setCursor(1,0);
        lcd.print("ID: (001-120)");
        lcd.setCursor(0,1);
        lcd.print(""); 
        lcd.display();

        nuevaHuella= false;
      }

      customKey = customKeypad.getKey();
    if (customKey)        // comprueba que se haya presionado una tecla                       
      {

      someInt = customKey - '0';
      posHuella[indice] = someInt;    // almacena en array la tecla presionada
      lcd.setCursor(indice,1);
      lcd.print(someInt);
      
      indice++;       // incrementa indice en uno
      }

      if(indice == 3){                                    // Pregunto y almaceno la posicion de la nueva huella

        numA= posHuella[0] * 100;
        numB= posHuella[1] * 10;
        numC= posHuella[2] * 1;

        numHuella= numA + numB + numC;

        if(1<= numHuella && numHuella <= 120){

          delay(500);

          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("ID Num: ");
          lcd.setCursor(0,1);
          lcd.print("NO = *    SI = #"); 
          lcd.setCursor(8,0);
          lcd.print(numHuella); 
          lcd.display();

          while( x == false){

            customKey = customKeypad.getKey();
            if (customKey){        // comprueba que se haya presionado una tecla
              if(customKey == '*'){

                indice= 0;
                someInt= 0;
                numHuella= 0;
                posHuella[0]= 0; posHuella[1]= 0; posHuella[2]= 0;
                nuevaHuella= true;
                x= true;
              }else if(customKey == '#'){

                registroH= false;                                         //Sale del loop
                indice= 0;
                someInt= 0;
                posHuella[0]= 0; posHuella[1]= 0; posHuella[2]= 0;
                x= true;
              }
            }
          }
          
          
          x= false;
          indice= 0;
          someInt= 0;
          posHuella[0]= 0; posHuella[1]= 0; posHuella[2]= 0;

          
        }else{

          delay(500);

          lcd.clear();
          lcd.setCursor(2,0);
          lcd.print("ERROR Numero");
          lcd.setCursor(0,1);
          lcd.print("Valores: 001-120"); 
          lcd.display();
          digitalWrite(buzzer, HIGH);
          delay(1000);
          digitalWrite(buzzer, LOW);
          delay(5000);

          indice= 0;
          someInt= 0;
          numHuella= 0;
          posHuella[0]= 0; posHuella[1]= 0; posHuella[2]= 0;
          nuevaHuella= true;
        }

        
      }    //End if indice = 3
      
    }   //END While(registroH)

      //**********FIN PREGUNTAR POSICION DE HUELLA A VERIFICAR**************

      //numHuella tiene el numero de la posicion ID

      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("*Autenticacion*");
      lcd.setCursor(0,1);
      lcd.print("Favor Poner Dedo"); 
      lcd.display();

      //**************VERIFICO SI LA HUELLA DEL USUARIO ES CORRECTA**************

      if (finger.begin()) {
        
        finger.readParams(&params);
        Serial.println("Found fingerprint sensor!");
        Serial.print("Capacity: "); Serial.println(params.capacity);
        Serial.print("Packet length: "); Serial.println(FPM::packet_lengths[params.packet_len]);

        match_prints(numHuella);

        finger.led_off();
    } else {
      
        Serial.println("Did not find fingerprint sensor :(");

        digitalWrite(buzzer, HIGH);
        delay(10000);
        digitalWrite(buzzer, LOW);
    }

    if(accesoHuella == true){

      for (byte i = 0; i < 4; i++) { // bucle recorre de a un byte por vez el UID

      switch(LecturaUID[i]){

      case  0: str+= "00";break; case 15: str+="0F";break; case 30: str+= "1E";break; case 45: str+="2D";break; case 60: str+= "3C";break; case 75: str+="4B";break; case  90: str+= "5A";break; case 105: str+="69";break; case 120: str+= "78";break; case 135: str+="87";break;
      case 150: str+= "96";break; case 165: str+= "A5";break; case 180: str+= "B4";break; case 195: str+= "C3";break; case 210: str+= "D2";break; case 225: str+= "E1";break; case 240: str+= "F0";break; case  1: str+= "01";break; case 16: str+="10";break; case 31: str+= "1F";break;
      case 46: str+="2E";break; case 61: str+= "3D";break; case 76: str+="4C";break; case  91: str+= "5B";break; case 106: str+="6A";break; case 121: str+= "79";break; case 136: str+="88";break; case 151: str+= "97";break; case 166: str+= "A6";break; case 181: str+= "B5";break;
      case 196: str+= "C4";break; case 211: str+= "D3";break; case 226: str+= "E2";break; case 241: str+= "F1";break; case  2: str+= "02";break; case 17: str+="11";break; case 32: str+= "20";break; case 47: str+="2F";break; case 62: str+= "3E";break; case 77: str+="4D";break;
      case  92: str+= "5C";break; case 107: str+="6B";break; case 122: str+= "7A";break; case 137: str+="89";break; case 152: str+= "98";break; case 167: str+= "A7";break; case 182: str+= "B6";break; case 197: str+= "C5";break; case 212: str+= "D4";break; case 227: str+= "E3";break;
      case 242: str+= "F2";break; case  3: str+= "03";break; case 18: str+="12";break; case 33: str+= "21";break; case 48: str+="30";break; case 63: str+= "3F";break; case 78: str+="4E";break; case  93: str+= "5D";break; case 108: str+="6C";break; case 123: str+= "7B";break;
      case 138: str+="8A";break; case 153: str+= "99";break; case 168: str+= "A8";break; case 183: str+= "B7";break; case 198: str+= "C6";break; case 213: str+= "D5";break; case 228: str+= "E4";break; case 243: str+= "F3";break; case  4: str+= "04";break; case 19: str+="13";break;
      case 34: str+= "22";break; case 49: str+="31";break; case 64: str+= "40";break; case 79: str+="4F";break; case  94: str+= "5E";break; case 109: str+="6D";break; case 124: str+= "7C";break; case 139: str+="8B";break; case 154: str+= "9A";break; case 169: str+= "A9";break;
      case 184: str+= "B8";break; case 199: str+= "C7";break; case 214: str+= "D6";break; case 229: str+= "E5";break; case 244: str+= "F4";break; case  5: str+= "05";break; case 20: str+="14";break; case 35: str+= "23";break; case 50: str+="32";break; case 65: str+= "41";break;
      case 80: str+="50";break; case  95: str+= "5F";break; case 110: str+="6E";break; case 125: str+= "7D";break; case 140: str+="8C";break; case 155: str+= "9B";break; case 170: str+= "AA";break; case 185: str+= "B9";break; case 200: str+= "C8";break; case 215: str+= "D7";break;
      case 230: str+= "E6";break; case 245: str+= "F5";break; case  6: str+= "06";break; case 21: str+="15";break; case 36: str+= "24";break; case 51: str+="33";break; case 66: str+= "42";break; case 81: str+="51";break; case  96: str+= "60";break; case 111: str+="6F";break;
      case 126: str+= "7E";break; case 141: str+="8D";break; case 156: str+= "9C";break; case 171: str+= "AB";break; case 186: str+= "BA";break; case 201: str+= "C9";break; case 216: str+= "D8";break; case 231: str+= "E7";break; case 246: str+= "F6";break; case  7: str+= "07";break;
      case 22: str+="16";break; case 37: str+= "25";break; case 52: str+="34";break; case 67: str+= "43";break; case 82: str+="52";break; case  97: str+= "61";break; case 112: str+="70";break; case 127: str+= "7F";break; case 142: str+="8E";break; case 157: str+= "9D";break;
      case 172: str+= "AC";break; case 187: str+= "BB";break; case 202: str+= "CA";break; case 217: str+= "D9";break; case 232: str+= "E8";break; case 247: str+= "F7";break; case  8: str+= "08";break; case 23: str+="17";break; case 38: str+= "26";break; case 53: str+="35";break;
      case 68: str+= "44";break; case 83: str+="53";break; case  98: str+= "62";break; case 113: str+="71";break; case 128: str+= "80";break; case 143: str+="8F";break; case 158: str+= "9E";break; case 173: str+= "AD";break; case 188: str+= "BC";break; case 203: str+= "CB";break;
      case 218: str+= "DA";break; case 233: str+= "E9";break; case 248: str+= "F8";break; case  9: str+= "09";break; case 24: str+="18";break; case 39: str+= "27";break; case 54: str+="36";break; case 69: str+= "45";break; case 84: str+="54";break; case  99: str+= "63";break;
      case 114: str+="72";break; case 129: str+= "81";break; case 144: str+="90";break; case 159: str+= "9F";break; case 174: str+= "AE";break; case 189: str+= "BD";break; case 204: str+= "CC";break; case 219: str+= "DB";break; case 234: str+= "EA";break; case 249: str+= "F9";break;
      case 10: str+= "0A";break; case 25: str+="19";break; case 40: str+= "28";break; case 55: str+="37";break; case 70: str+= "46";break; case 85: str+="55";break; case 100: str+= "64";break; case 115: str+="73";break; case 130: str+= "82";break; case 145: str+="91";break;
      case 160: str+= "A0";break; case 175: str+= "AF";break; case 190: str+= "BE";break; case 205: str+= "CD";break; case 220: str+= "DC";break; case 235: str+= "EB";break; case 250: str+= "FA";break; case 11: str+= "0B";break; case 26: str+="1A";break; case 41: str+= "29";break;
      case 56: str+="38";break; case 71: str+= "47";break; case 86: str+="56";break; case 101: str+= "65";break; case 116: str+="74";break; case 131: str+= "83";break; case 146: str+="92";break; case 161: str+= "A1";break; case 176: str+= "B0";break; case 191: str+= "BF";break;
      case 206: str+= "CE";break; case 221: str+= "DD";break; case 236: str+= "EC";break; case 251: str+= "FB";break; case 12: str+= "0C";break; case 27: str+="1B";break; case 42: str+= "2A";break; case 57: str+="39";break; case 72: str+= "48";break; case 87: str+="57";break;
      case 102: str+= "66";break; case 117: str+="75";break; case 132: str+= "84";break; case 147: str+="93";break; case 162: str+= "A2";break; case 177: str+= "B1";break; case 192: str+= "C0";break; case 207: str+= "CF";break; case 222: str+= "DE";break; case 237: str+= "ED";break;
      case 252: str+= "FC";break; case 13: str+= "0D";break; case 28: str+="1C";break; case 43: str+= "2B";break; case 58: str+="3A";break; case 73: str+= "49";break; case 88: str+="58";break; case 103: str+= "67";break; case 118: str+="76";break; case 133: str+= "85";break;
      case 148: str+="94";break; case 163: str+= "A3";break; case 178: str+= "B2";break; case 193: str+= "C1";break; case 208: str+= "D0";break; case 223: str+= "DF";break; case 238: str+= "EE";break; case 253: str+= "FD";break; case 14: str+= "0E";break; case 29: str+="1D";break;
      case 44: str+= "2C";break; case 59: str+="3B";break; case 74: str+= "4A";break; case 89: str+="59";break; case 104: str+= "68";break; case 119: str+="77";break; case 134: str+= "86";break; case 149: str+="95";break; case 164: str+= "A4";break; case 179: str+= "B3";break;
      case 194: str+= "C2";break; case 209: str+= "D1";break; case 224: str+= "E0";break; case 239: str+= "EF";break; case 254: str+= "FE";break; case 255: str+= "FF";break;
      }

      contadorLan+= 1;

      if(contadorLan<4){

        str+= ":";
      }
      
      }//Fin del for

      contadorLan= 0;
      operacion= "Ingreso";
      seguridad= "Con_Huella";

      
      if(client.connect(sever,80)){                             //Envio de datos por red LAN
        Serial.println("Conexion LAN realizada.");
        client.print("GET /arduino/readsensor.php?");
        client.print("Operacion=");
        client.print(operacion);
        client.print("&Usuario=");
        client.print(str);
        client.print("&Seguridad=");
        client.println(seguridad);

        Serial.print("Usuario: ");
        Serial.println(str);

        client.stop();
        client.flush();
     }
     else{
      
          Serial.println("No se pudo establecer conexion LAN.");

          lcd.clear();
          lcd.setCursor(2,0);
          lcd.print("Fallo En La");
          lcd.setCursor(2,1);
          lcd.print("Conexion LAN");
          lcd.display();

          delay(500);
          
         client.stop();
         client.flush();
     }

      operacion= "";
      str= "";
      seguridad= "";
      
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Acceso Concedido");
      lcd.setCursor(3,1);
      lcd.print("Bienvenido"); 
      lcd.display();

      resetHuella= false;
    
      digitalWrite(buzzer, HIGH);          //Pitido de ACCESO CONCEDIDO***
      delay(50);
      digitalWrite(buzzer, LOW);
      delay(50);
      digitalWrite(buzzer, HIGH);
      delay(50);
      digitalWrite(buzzer, LOW);

      digitalWrite(rele, LOW);           //Concedo acceso con RELE
      delay(50);
      digitalWrite(rele, HIGH);
      delay(1000);

      accesoHuella= false;
      displayHome= true;
    }else{
      
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Acceso Denegado");
      lcd.setCursor(1,1);
      lcd.print("Huella Erronea"); 
      lcd.display();

      digitalWrite(buzzer, HIGH);
      delay(1000);
      digitalWrite(buzzer, LOW);
      delay(2000);
      
      displayHome= true;
      }
      
      }else{

      for (byte i = 0; i < 4; i++) { // bucle recorre de a un byte por vez el UID

      switch(LecturaUID[i]){

      case  0: str+= "00";break; case 15: str+="0F";break; case 30: str+= "1E";break; case 45: str+="2D";break; case 60: str+= "3C";break; case 75: str+="4B";break; case  90: str+= "5A";break; case 105: str+="69";break; case 120: str+= "78";break; case 135: str+="87";break;
      case 150: str+= "96";break; case 165: str+= "A5";break; case 180: str+= "B4";break; case 195: str+= "C3";break; case 210: str+= "D2";break; case 225: str+= "E1";break; case 240: str+= "F0";break; case  1: str+= "01";break; case 16: str+="10";break; case 31: str+= "1F";break;
      case 46: str+="2E";break; case 61: str+= "3D";break; case 76: str+="4C";break; case  91: str+= "5B";break; case 106: str+="6A";break; case 121: str+= "79";break; case 136: str+="88";break; case 151: str+= "97";break; case 166: str+= "A6";break; case 181: str+= "B5";break;
      case 196: str+= "C4";break; case 211: str+= "D3";break; case 226: str+= "E2";break; case 241: str+= "F1";break; case  2: str+= "02";break; case 17: str+="11";break; case 32: str+= "20";break; case 47: str+="2F";break; case 62: str+= "3E";break; case 77: str+="4D";break;
      case  92: str+= "5C";break; case 107: str+="6B";break; case 122: str+= "7A";break; case 137: str+="89";break; case 152: str+= "98";break; case 167: str+= "A7";break; case 182: str+= "B6";break; case 197: str+= "C5";break; case 212: str+= "D4";break; case 227: str+= "E3";break;
      case 242: str+= "F2";break; case  3: str+= "03";break; case 18: str+="12";break; case 33: str+= "21";break; case 48: str+="30";break; case 63: str+= "3F";break; case 78: str+="4E";break; case  93: str+= "5D";break; case 108: str+="6C";break; case 123: str+= "7B";break;
      case 138: str+="8A";break; case 153: str+= "99";break; case 168: str+= "A8";break; case 183: str+= "B7";break; case 198: str+= "C6";break; case 213: str+= "D5";break; case 228: str+= "E4";break; case 243: str+= "F3";break; case  4: str+= "04";break; case 19: str+="13";break;
      case 34: str+= "22";break; case 49: str+="31";break; case 64: str+= "40";break; case 79: str+="4F";break; case  94: str+= "5E";break; case 109: str+="6D";break; case 124: str+= "7C";break; case 139: str+="8B";break; case 154: str+= "9A";break; case 169: str+= "A9";break;
      case 184: str+= "B8";break; case 199: str+= "C7";break; case 214: str+= "D6";break; case 229: str+= "E5";break; case 244: str+= "F4";break; case  5: str+= "05";break; case 20: str+="14";break; case 35: str+= "23";break; case 50: str+="32";break; case 65: str+= "41";break;
      case 80: str+="50";break; case  95: str+= "5F";break; case 110: str+="6E";break; case 125: str+= "7D";break; case 140: str+="8C";break; case 155: str+= "9B";break; case 170: str+= "AA";break; case 185: str+= "B9";break; case 200: str+= "C8";break; case 215: str+= "D7";break;
      case 230: str+= "E6";break; case 245: str+= "F5";break; case  6: str+= "06";break; case 21: str+="15";break; case 36: str+= "24";break; case 51: str+="33";break; case 66: str+= "42";break; case 81: str+="51";break; case  96: str+= "60";break; case 111: str+="6F";break;
      case 126: str+= "7E";break; case 141: str+="8D";break; case 156: str+= "9C";break; case 171: str+= "AB";break; case 186: str+= "BA";break; case 201: str+= "C9";break; case 216: str+= "D8";break; case 231: str+= "E7";break; case 246: str+= "F6";break; case  7: str+= "07";break;
      case 22: str+="16";break; case 37: str+= "25";break; case 52: str+="34";break; case 67: str+= "43";break; case 82: str+="52";break; case  97: str+= "61";break; case 112: str+="70";break; case 127: str+= "7F";break; case 142: str+="8E";break; case 157: str+= "9D";break;
      case 172: str+= "AC";break; case 187: str+= "BB";break; case 202: str+= "CA";break; case 217: str+= "D9";break; case 232: str+= "E8";break; case 247: str+= "F7";break; case  8: str+= "08";break; case 23: str+="17";break; case 38: str+= "26";break; case 53: str+="35";break;
      case 68: str+= "44";break; case 83: str+="53";break; case  98: str+= "62";break; case 113: str+="71";break; case 128: str+= "80";break; case 143: str+="8F";break; case 158: str+= "9E";break; case 173: str+= "AD";break; case 188: str+= "BC";break; case 203: str+= "CB";break;
      case 218: str+= "DA";break; case 233: str+= "E9";break; case 248: str+= "F8";break; case  9: str+= "09";break; case 24: str+="18";break; case 39: str+= "27";break; case 54: str+="36";break; case 69: str+= "45";break; case 84: str+="54";break; case  99: str+= "63";break;
      case 114: str+="72";break; case 129: str+= "81";break; case 144: str+="90";break; case 159: str+= "9F";break; case 174: str+= "AE";break; case 189: str+= "BD";break; case 204: str+= "CC";break; case 219: str+= "DB";break; case 234: str+= "EA";break; case 249: str+= "F9";break;
      case 10: str+= "0A";break; case 25: str+="19";break; case 40: str+= "28";break; case 55: str+="37";break; case 70: str+= "46";break; case 85: str+="55";break; case 100: str+= "64";break; case 115: str+="73";break; case 130: str+= "82";break; case 145: str+="91";break;
      case 160: str+= "A0";break; case 175: str+= "AF";break; case 190: str+= "BE";break; case 205: str+= "CD";break; case 220: str+= "DC";break; case 235: str+= "EB";break; case 250: str+= "FA";break; case 11: str+= "0B";break; case 26: str+="1A";break; case 41: str+= "29";break;
      case 56: str+="38";break; case 71: str+= "47";break; case 86: str+="56";break; case 101: str+= "65";break; case 116: str+="74";break; case 131: str+= "83";break; case 146: str+="92";break; case 161: str+= "A1";break; case 176: str+= "B0";break; case 191: str+= "BF";break;
      case 206: str+= "CE";break; case 221: str+= "DD";break; case 236: str+= "EC";break; case 251: str+= "FB";break; case 12: str+= "0C";break; case 27: str+="1B";break; case 42: str+= "2A";break; case 57: str+="39";break; case 72: str+= "48";break; case 87: str+="57";break;
      case 102: str+= "66";break; case 117: str+="75";break; case 132: str+= "84";break; case 147: str+="93";break; case 162: str+= "A2";break; case 177: str+= "B1";break; case 192: str+= "C0";break; case 207: str+= "CF";break; case 222: str+= "DE";break; case 237: str+= "ED";break;
      case 252: str+= "FC";break; case 13: str+= "0D";break; case 28: str+="1C";break; case 43: str+= "2B";break; case 58: str+="3A";break; case 73: str+= "49";break; case 88: str+="58";break; case 103: str+= "67";break; case 118: str+="76";break; case 133: str+= "85";break;
      case 148: str+="94";break; case 163: str+= "A3";break; case 178: str+= "B2";break; case 193: str+= "C1";break; case 208: str+= "D0";break; case 223: str+= "DF";break; case 238: str+= "EE";break; case 253: str+= "FD";break; case 14: str+= "0E";break; case 29: str+="1D";break;
      case 44: str+= "2C";break; case 59: str+="3B";break; case 74: str+= "4A";break; case 89: str+="59";break; case 104: str+= "68";break; case 119: str+="77";break; case 134: str+= "86";break; case 149: str+="95";break; case 164: str+= "A4";break; case 179: str+= "B3";break;
      case 194: str+= "C2";break; case 209: str+= "D1";break; case 224: str+= "E0";break; case 239: str+= "EF";break; case 254: str+= "FE";break; case 255: str+= "FF";break;
      }

      contadorLan+= 1;

      if(contadorLan<4){

        str+= ":";
      }
    
      }//Fin del for

      contadorLan= 0;
      operacion= "Ingreso";
      seguridad= "Sin_Huella";
      
      if(client.connect(sever,80)){                             //Envio de datos por red LAN
        Serial.println("Conexion LAN realizada.");
        client.print("GET /arduino/readsensor.php?");
        client.print("Operacion=");
        client.print(operacion);
        client.print("&Usuario=");
        client.print(str);
        client.print("&Seguridad=");
        client.println(seguridad);

        Serial.print("Usuario: ");
        Serial.println(str);

        client.stop();
        client.flush();
     }
     else{
      
          Serial.println("No se pudo establecer conexion LAN.");

          lcd.clear();
          lcd.setCursor(2,0);
          lcd.print("Fallo En La");
          lcd.setCursor(2,1);
          lcd.print("Conexion LAN");
          lcd.display();

          delay(500);
          
         client.stop();
         client.flush();
     }

      operacion= "";
      str= "";
      seguridad= "";

      
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Acceso Concedido");
      lcd.setCursor(3,1);
      lcd.print("Bienvenido"); 
      lcd.display();
    
      digitalWrite(buzzer, HIGH);          //Pitido de ACCESO CONCEDIDO***
      delay(50);
      digitalWrite(buzzer, LOW);
      delay(50);
      digitalWrite(buzzer, HIGH);
      delay(50);
      digitalWrite(buzzer, LOW);

      digitalWrite(rele, LOW);           //Concedo acceso con RELE
      delay(50);
      digitalWrite(rele, HIGH);
      delay(1000);
      
      displayHome= true;
    }
    
    }else{

      lcd.clear();
      lcd.setCursor(4,0);
      lcd.print("*ERROR*");
      lcd.setCursor(0,1);
      lcd.print("Acceso Denegado"); 
      lcd.display();

      
      digitalWrite(buzzer, HIGH);          //Pitido de ACCESO DENEGADO***
      delay(1000);
      digitalWrite(buzzer, LOW);
      delay(2000);
      
      displayHome= true;
      }

  accesoRFID= false;         
  }



boolean comparaUID(byte lectura[],byte usuario[]){  // funcion comparaUID
  for (byte i=0; i < mfrc522.uid.size; i++){    // bucle recorre de a un byte por vez el UID
  if(lectura[i] != usuario[i])        // si byte de UID leido es distinto a usuario
    return(false);          // retorna falso
  }
  return(true);           // si los 4 bytes coinciden retorna verdadero
}



void registrarHuella(){

  nuevaHuella= true;
  registroH= true;

  while(registroH == true){

    if(nuevaHuella== true){

      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("Nuevo Registro");
      lcd.setCursor(3,1);
      lcd.print("De Huella"); 
      lcd.display();
      delay(3000);
      nuevaHuella= false;

      digitalWrite(buzzer, HIGH);
      delay(50);
      digitalWrite(buzzer, LOW);
      delay(50);
      digitalWrite(buzzer, HIGH);
      delay(50);
      digitalWrite(buzzer, LOW);
    
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Ingrese Posicion");
      lcd.setCursor(0,1);
      lcd.print(""); 
      lcd.display();
    }
    
    customKey = customKeypad.getKey();
    if (customKey)        // comprueba que se haya presionado una tecla
      {

      someInt = customKey - '0';
      posHuella[indice] = someInt;    // almacena en array la tecla presionada
      lcd.setCursor(indice,1);
      lcd.print(someInt);
      
      indice++;       // incrementa indice en uno
      }

      if(indice == 3){                                    // Pregunto y almaceno la posicion de la nueva huella

        numA= posHuella[0] * 100;
        numB= posHuella[1] * 10;
        numC= posHuella[2] * 1;

        numHuella= numA + numB + numC;

        if(1<= numHuella && numHuella <= 120){

          delay(500);

          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("POSICION: ");
          lcd.setCursor(0,1);
          lcd.print("NO = *    SI = #"); 
          lcd.setCursor(10,0);
          lcd.print(numHuella); 
          lcd.display();

          while( x == false){

            customKey = customKeypad.getKey();
            if (customKey){        // comprueba que se haya presionado una tecla
              if(customKey == '*'){

                indice= 0;
                someInt= 0;
                numHuella= 0;
                posHuella[0]= 0; posHuella[1]= 0; posHuella[2]= 0;
                nuevaHuella= true;
                x= true;
              }else if(customKey == '#'){

                registroH= false;
                indice= 0;
                someInt= 0;
                posHuella[0]= 0; posHuella[1]= 0; posHuella[2]= 0;
                x= true;
              }
            }
          }
          
          
          x= false;
          indice= 0;
          someInt= 0;
          posHuella[0]= 0; posHuella[1]= 0; posHuella[2]= 0;

          
        }else{

          delay(500);

          lcd.clear();
          lcd.setCursor(2,0);
          lcd.print("ERROR Numero");
          lcd.setCursor(0,1);
          lcd.print("Valores: 001-120"); 
          lcd.display();
          digitalWrite(buzzer, HIGH);
          delay(1000);
          digitalWrite(buzzer, LOW);
          delay(5000);

          indice= 0;
          someInt= 0;
          numHuella= 0;
          posHuella[0]= 0; posHuella[1]= 0; posHuella[2]= 0;
          nuevaHuella= true;
        }

        
      }
    
  }
    
    if (finger.begin()) {

      Serial.println("Found fingerprint sensor!");

      if(1<= numHuella <= 120){                           //*************AQUI RECIBO LA POSICION Y REGISTRO LA NUEVA HUELLA**************

      fid= numHuella;

      numHuella= 0;

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Registro Huella");
      lcd.setCursor(0,1);
      lcd.print("Favor Poner Dedo"); 
      lcd.display();

      enroll_finger(fid);

      finger.led_off();

      digitalWrite(buzzer, HIGH);       //DOBLE PITIDO
      delay(50);
      digitalWrite(buzzer, LOW);
      delay(50);
      digitalWrite(buzzer, HIGH);
      delay(50);
      digitalWrite(buzzer, LOW);

      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("Reg. De Huella");
      lcd.setCursor(0,1);
      lcd.print("Ha Sido Exitoso"); 
      lcd.display();
      delay(3000);
      
    }
  }else {

      Serial.println("Did not find fingerprint sensor :(");
      
      digitalWrite(buzzer, HIGH);
      delay(10000);
      digitalWrite(buzzer, LOW);
  } 
}



//ENROLL
int16_t enroll_finger(int16_t fid) {
    int16_t p = -1;
    Serial.println("Waiting for valid finger to enroll");
    while (p != FPM_OK) {
        p = finger.getImage();
        switch (p) {
            case FPM_OK:
                Serial.println("Image taken");
                break;
            case FPM_NOFINGER:
                Serial.println(".");
                break;
            case FPM_PACKETRECIEVEERR:
                Serial.println("Communication error");
                break;
            case FPM_IMAGEFAIL:
                Serial.println("Imaging error");
                break;
            case FPM_TIMEOUT:
                Serial.println("Timeout!");
                break;
            case FPM_READ_ERROR:
                Serial.println("Got wrong PID or length!");
                break;
            default:
                Serial.println("Unknown error");
                break;
        }
        yield();
    }
    // OK success!

    p = finger.image2Tz(1);
    switch (p) {
        case FPM_OK:
            Serial.println("Image converted");
            break;
        case FPM_IMAGEMESS:
            Serial.println("Image too messy");
            return p;
        case FPM_PACKETRECIEVEERR:
            Serial.println("Communication error");
            return p;
        case FPM_FEATUREFAIL:
            Serial.println("Could not find fingerprint features");
            return p;
        case FPM_INVALIDIMAGE:
            Serial.println("Could not find fingerprint features");
            return p;
        case FPM_TIMEOUT:
            Serial.println("Timeout!");
            return p;
        case FPM_READ_ERROR:
            Serial.println("Got wrong PID or length!");
            return p;
        default:
            Serial.println("Unknown error");
            return p;
    }

    Serial.println("Remove finger");

    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);
    delay(50);
    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);

    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Favor Quitar");
    lcd.setCursor(4,1);
    lcd.print("El Dedo"); 
    lcd.display();
    delay(5000);
    
    
    p = 0;
    while (p != FPM_NOFINGER) {
        p = finger.getImage();
        yield();
    }

    p = -1;
    Serial.println("Place same finger again");

    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);
    delay(50);
    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);

    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Favor Poner");
    lcd.setCursor(1,1);
    lcd.print("El Mismo Dedo"); 
    lcd.display();
    
    while (p != FPM_OK) {
        p = finger.getImage();
        switch (p) {
            case FPM_OK:
                Serial.println("Image taken");
                break;
            case FPM_NOFINGER:
                Serial.print(".");
                break;
            case FPM_PACKETRECIEVEERR:
                Serial.println("Communication error");
                break;
            case FPM_IMAGEFAIL:
                Serial.println("Imaging error");
                break;
            case FPM_TIMEOUT:
                Serial.println("Timeout!");
                break;
            case FPM_READ_ERROR:
                Serial.println("Got wrong PID or length!");
                break;
            default:
                Serial.println("Unknown error");
                break;
        }
        yield();
    }

    // OK success!

    p = finger.image2Tz(2);
    switch (p) {
        case FPM_OK:
            Serial.println("Image converted");
            break;
        case FPM_IMAGEMESS:
            Serial.println("Image too messy");
            return p;
        case FPM_PACKETRECIEVEERR:
            Serial.println("Communication error");
            return p;
        case FPM_FEATUREFAIL:
            Serial.println("Could not find fingerprint features");
            return p;
        case FPM_INVALIDIMAGE:
            Serial.println("Could not find fingerprint features");
            return p;
        case FPM_TIMEOUT:
            Serial.println("Timeout!");
            return false;
        case FPM_READ_ERROR:
            Serial.println("Got wrong PID or length!");
            return false;
        default:
            Serial.println("Unknown error");
            return p;
    }


    // OK converted!
    p = finger.createModel();
    if (p == FPM_OK) {
        Serial.println("Prints matched!");
    } else if (p == FPM_PACKETRECIEVEERR) {
        Serial.println("Communication error");
        return p;
    } else if (p == FPM_ENROLLMISMATCH) {
        Serial.println("Fingerprints did not match");
        return p;
    } else if (p == FPM_TIMEOUT) {
        Serial.println("Timeout!");
        return p;
    } else if (p == FPM_READ_ERROR) {
        Serial.println("Got wrong PID or length!");
        return p;
    } else {
        Serial.println("Unknown error");
        return p;
    }

    Serial.print("ID "); Serial.println(fid);
    p = finger.storeModel(fid);
    if (p == FPM_OK) {
        Serial.println("Stored!");
        return 0;
    } else if (p == FPM_PACKETRECIEVEERR) {
        Serial.println("Communication error");
        return p;
    } else if (p == FPM_BADLOCATION) {
        Serial.println("Could not store in that location");
        return p;
    } else if (p == FPM_FLASHERR) {
        Serial.println("Error writing to flash");
        return p;
    } else if (p == FPM_TIMEOUT) {
        Serial.println("Timeout!");
        return p;
    } else if (p == FPM_READ_ERROR) {
        Serial.println("Got wrong PID or length!");
        return p;
    } else {
        Serial.println("Unknown error");
        return p;
    }
}



void eliminarUsuario(){

  lcd.clear();
  lcd.setCursor(3,0);
  lcd.print("MODO ADMIN");
  lcd.setCursor(0,1);
  lcd.print(":Eliminar User:"); 
  lcd.display();
  delay(1000);

  modoAdministrador();                 //Compruebo permiso de administrador

  if(modoAdmin == true){

      digitalWrite(buzzer, HIGH);       //DOBLE PITIDO
      delay(50);
      digitalWrite(buzzer, LOW);
      delay(50);
      digitalWrite(buzzer, HIGH);
      delay(50);
      digitalWrite(buzzer, LOW);
      
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ACCESO CONCEDIDO");
      lcd.setCursor(1,1);
      lcd.print("ADMINISTRADOR"); 
      lcd.display();
      delay(3000);

    
    displayRfid= true;

    while(errorLectura == false){                                                 //Proceso de eliminacion de usuario

    if(displayRfid == true){

      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("Favor Colocar");
      lcd.setCursor(0,1);
      lcd.print("Tarj. a Eliminar"); 
      lcd.display();

      displayRfid= false;
    }

    digitalWrite(led_rojo, HIGH);
    
  if (mfrc522.PICC_IsNewCardPresent()){   // si hay una tarjeta presente
    if (mfrc522.PICC_ReadCardSerial()){ // si puede obtener datos de la tarjeta

        Serial.print("Tarjeta UID:");       // muestra texto UID:
        for (byte i = 0; i < 4; i++) { // bucle recorre de a un byte por vez el UID
      if (mfrc522.uid.uidByte[i] < 0x10){   // si el byte leido es menor a 0x10
        Serial.print(" 0");       // imprime espacio en blanco y numero cero
        }
        else{           // sino
          Serial.print(" ");        // imprime un espacio en blanco
          }
          Serial.print(mfrc522.uid.uidByte[i], HEX);    // imprime el byte del UID leido en hexadecimal
          LecturaUID[i]=mfrc522.uid.uidByte[i];     // almacena en array el byte del UID leido      
          }
          
          Serial.println("");         // imprime un espacio
          errorLectura= true;      
      } }  

  }

  mfrc522.PICC_HaltA();               // detiene comunicacion con tarjeta
  errorLectura= false;
  digitalWrite(led_rojo, LOW);        //Apago el led de lectura

  //Ya tengo un usuario leido y almacenado temporalmente

      digitalWrite(buzzer, HIGH);
      delay(50);
      digitalWrite(buzzer, LOW);
      delay(50);
      digitalWrite(buzzer, HIGH);
      delay(50);
      digitalWrite(buzzer, LOW);
      
      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("Tarjeta Leida");
      lcd.setCursor(1,1);
      lcd.print("Correctamente"); 
      lcd.display();
      delay(2000);

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Borrar Registro");
      lcd.setCursor(3,1);
      lcd.print("De Huella"); 
      lcd.display();
      delay(2000);
    
      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("Lugar (001-120)");
      lcd.setCursor(0,1);
      lcd.print(""); 
      lcd.display();

    registroH= true;

    while(registroH == true){                                                   //Leo los tres numeros de posicion

      if(nuevaHuella== true){

        digitalWrite(buzzer, HIGH);
        delay(50);
        digitalWrite(buzzer, LOW);
        delay(50);
        digitalWrite(buzzer, HIGH);
        delay(50);
        digitalWrite(buzzer, LOW);

        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Borrar Registro");
        lcd.setCursor(3,1);
        lcd.print("De Huella"); 
        lcd.display();
        delay(2000);
    
        lcd.clear();
        lcd.setCursor(1,0);
        lcd.print("Lugar (001-120)");
        lcd.setCursor(0,1);
        lcd.print(""); 
        lcd.display();

        nuevaHuella= false;
      }

      customKey = customKeypad.getKey();
    if (customKey)        // comprueba que se haya presionado una tecla                       
      {

      someInt = customKey - '0';
      posHuella[indice] = someInt;    // almacena en array la tecla presionada
      lcd.setCursor(indice,1);
      lcd.print(someInt);
      
      indice++;       // incrementa indice en uno
      }

      if(indice == 3){                                    // Pregunto y almaceno la posicion de la nueva huella

        numA= posHuella[0] * 100;
        numB= posHuella[1] * 10;
        numC= posHuella[2] * 1;

        numHuella= numA + numB + numC;

        if(1<= numHuella && numHuella <= 120){

          delay(500);

          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("POSICION: ");
          lcd.setCursor(0,1);
          lcd.print("NO = *    SI = #"); 
          lcd.setCursor(10,0);
          lcd.print(numHuella); 
          lcd.display();

          while( x == false){

            customKey = customKeypad.getKey();
            if (customKey){        // comprueba que se haya presionado una tecla
              if(customKey == '*'){

                indice= 0;
                someInt= 0;
                numHuella= 0;
                posHuella[0]= 0; posHuella[1]= 0; posHuella[2]= 0;
                nuevaHuella= true;
                x= true;
              }else if(customKey == '#'){

                registroH= false;                                         //Sale del loop
                indice= 0;
                someInt= 0;
                posHuella[0]= 0; posHuella[1]= 0; posHuella[2]= 0;
                x= true;
              }
            }
          }
          
          
          x= false;
          indice= 0;
          someInt= 0;
          posHuella[0]= 0; posHuella[1]= 0; posHuella[2]= 0;

          
        }else{

          delay(500);

          lcd.clear();
          lcd.setCursor(2,0);
          lcd.print("ERROR Numero");
          lcd.setCursor(0,1);
          lcd.print("Valores: 001-120"); 
          lcd.display();
          digitalWrite(buzzer, HIGH);
          delay(1000);
          digitalWrite(buzzer, LOW);
          delay(5000);

          indice= 0;
          someInt= 0;
          numHuella= 0;
          posHuella[0]= 0; posHuella[1]= 0; posHuella[2]= 0;
          nuevaHuella= true;
        }

        
      }    //End if indice = 3
      
    }   //END While(registroH)

                                      //En este punto ya tengo el usuario a eliminar y la posicion de la huella a eliminar

      for (int i = 0; i <= EEPROM.length(); i++){                  //Comparar el UID a eliminar con la informacion en la EEPROM

    
    if(LecturaUID[contadorEEPROM]== EEPROM.read(i)){
      
      contadorEEPROM= contadorEEPROM + 1;
      i= i+1;
      if(LecturaUID[contadorEEPROM]== EEPROM.read(i)){

        contadorEEPROM= contadorEEPROM + 1;
        i= i+1;
        if(LecturaUID[contadorEEPROM]== EEPROM.read(i)){

          contadorEEPROM= contadorEEPROM + 1;
          i= i+1;
          if(LecturaUID[contadorEEPROM]== EEPROM.read(i)){
            
            //Si lo encuentra
            EEPROM.put(i-3, Eliminar);
            
            contadorEEPROM= 0;
            i= EEPROM.length();
            }
          }        
        }      
      }else{ contadorEEPROM= 0;}
    }

    numHuella2 = numHuella;                                       //Eliminacion de la posicion de la huella
    Serial.print("Posicion de huella a eliminar: ");
    Serial.println(numHuella2);
    
    deleteFingerprint(numHuella2);

    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);
    delay(50);
    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);

    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("User Eliminado");
    lcd.setCursor(1,1);
    lcd.print("Correctamente"); 
    lcd.display();
    delay(5000);
      
    displayHome= true;
    modoAdmin= false;
    }else{

      lcd.clear();
      lcd.setCursor(4,0);
      lcd.print("*ERROR*");
      lcd.setCursor(0,1);
      lcd.print("Usuario Denegado"); 
      lcd.display();

      digitalWrite(buzzer, HIGH);
      delay(1000);
      digitalWrite(buzzer, LOW);
      delay(3000);

      displayHome= true;
    }
} //FIN FUNCION ELIMINAR USUARIO



void limpiarTodo(){

  modoAdministrador();

  if(modoAdmin == true){

    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);
    delay(50);
    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);

    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Limpiar Todo");
    lcd.setCursor(3,1);
    lcd.print("El Sistema"); 
    lcd.display();
    delay(3000);

    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);
    delay(50);
    digitalWrite(buzzer, HIGH);
    delay(50);
    digitalWrite(buzzer, LOW);

    displayLimpiar = true;

    while( x == false){

      if(displayLimpiar == true){

        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("INGRESAR CLAVE:");
        lcd.setCursor(0,1);
        lcd.print(""); 
        lcd.display();

        displayLimpiar= false;
      }

      
      customKey = customKeypad.getKey();

      if (customKey){

        if(customKey == '0' || customKey == '1' || customKey == '2' || customKey == '3' || customKey == '4' || customKey == '5' ||
          customKey == '6' || customKey == '7' || customKey == '8' || customKey == '9'){

          claveAdmin += customKey;
        
          lcd.setCursor(contadorClave,1);
          lcd.print("*");
        
          contadorClave += 1;
        
          if(contadorClave == 8){

           comprobarClave= String(claveAdmin).toInt();
           verificarClave= true;
           delay(500);
           }
        }
      }

      if(verificarClave == true){

        if(comprobarClave == contrasenaAdmin){

          lcd.clear();
          lcd.setCursor(1,0);
          lcd.print("CLAVE CORRECTA");
          lcd.setCursor(1,1);
          lcd.print("ELIMINANDO..."); 
          lcd.display();

          contrasenaBien = true;
          x= true;
          claveAdmin= "";
          comprobarClave= 0;
          contadorClave= 0;
        }else{

          lcd.clear();
          lcd.setCursor(2,0);
          lcd.print("ERROR CLAVE");
          lcd.setCursor(4,1);
          lcd.print("ERRONEA"); 
          lcd.display();

          digitalWrite(buzzer, HIGH);
          delay(1000);
          digitalWrite(buzzer, LOW);
          delay(2000);

          x= true;
          claveAdmin= "";
          comprobarClave= 0;
          contadorClave= 0;
        }

        verificarClave= false;
      }
    }
    x= false;

    if(contrasenaBien == true){

      for (int i = 0; i <= EEPROM.length(); i++){

        EEPROM.put(i, eliminarEEPROM);
        delay(5);
      }

      empty_database();
      delay(5000);

      digitalWrite(buzzer, HIGH);
      delay(50);
      digitalWrite(buzzer, LOW);
      delay(50);
      digitalWrite(buzzer, HIGH);
      delay(50);
      digitalWrite(buzzer, LOW);

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Datos Eliminados");
      lcd.setCursor(0,1);
      lcd.print("Correctamente :)"); 
      lcd.display();
      delay(3000);
    
    }

    modoAdmin= false;
  }else{

    lcd.clear();
    lcd.setCursor(4,0);
    lcd.print("*ERROR*");
    lcd.setCursor(0,1);
    lcd.print("Usuario Denegado"); 
    lcd.display();

    digitalWrite(buzzer, HIGH);
    delay(1000);
    digitalWrite(buzzer, LOW);
    delay(3000);
  }

  displayHome= true;
}


void match_prints(int16_t fid) {
    int16_t p = -1;

    /* first get the finger image */
    Serial.println("Waiting for valid finger");
    while (p != FPM_OK) {
        p = finger.getImage();
        switch (p) {
            case FPM_OK:
                Serial.println("Image taken");
                break;
            case FPM_NOFINGER:
                Serial.print(".");
                break;
            case FPM_PACKETRECIEVEERR:
                Serial.println("Communication error");
                return;
            default:
                Serial.println("Unknown error");
                return;
        }
        yield();
    }

    /* convert it and place in slot 1*/
    p = finger.image2Tz(1);
    switch (p) {
        case FPM_OK:
            Serial.println("Image converted");
            break;
        case FPM_IMAGEMESS:
            Serial.println("Image too messy");
            return;
        case FPM_PACKETRECIEVEERR:
            Serial.println("Communication error");
            return;
        default:
            Serial.println("Unknown error");
            return;
    }

    Serial.println("Remove finger");
    p = 0;
    while (p != FPM_NOFINGER) {
        p = finger.getImage();
        yield();
    }
    Serial.println();

    /* read template into slot 2 */
    p = finger.loadModel(fid, 2);
    switch (p) {
        case FPM_OK:
            Serial.print("Template "); Serial.print(fid); Serial.println(" loaded.");
            break;
        case FPM_PACKETRECIEVEERR:
            Serial.println("Communication error");
            return;
        case FPM_DBREADFAIL:
            Serial.println("Invalid template");
            return;
        default:
            Serial.print("Unknown error "); Serial.println(p);
            return;
    }
    
    uint16_t match_score = 0;
    p = finger.matchTemplatePair(&match_score);
    switch (p) {
        case FPM_OK:
            Serial.print("Prints matched. Score: "); Serial.println(match_score);
            accesoHuella= true;
            break;
        case FPM_NOMATCH:
            Serial.println("Prints did not match.");
            break;
        default:
            Serial.println("Unknown error");
            return;
    }
}



int deleteFingerprint(int fid) {
    int p = -1;

    p = finger.deleteModel(fid);

    if (p == FPM_OK) {
        Serial.println("Deleted!");
    } else if (p == FPM_PACKETRECIEVEERR) {
        Serial.println("Communication error");
        return p;
    } else if (p == FPM_BADLOCATION) {
        Serial.println("Could not delete in that location");
        return p;
    } else if (p == FPM_FLASHERR) {
        Serial.println("Error writing to flash");
        return p;
    } else if (p == FPM_TIMEOUT) {
        Serial.println("Timeout!");
        return p;
    } else if (p == FPM_READ_ERROR) {
        Serial.println("Got wrong PID or length!");
        return p;
    } else {
        Serial.print("Unknown error: 0x"); Serial.println(p, HEX);
        return p;
    }
}



void empty_database(void) {
    int16_t p = finger.emptyDatabase();
    if (p == FPM_OK) {
        Serial.println("Database empty!");
    }
    else if (p == FPM_PACKETRECIEVEERR) {
        Serial.print("Communication error!");
    }
    else if (p == FPM_DBCLEARFAIL) {
        Serial.println("Could not clear database!");
    } 
    else if (p == FPM_TIMEOUT) {
        Serial.println("Timeout!");
    } 
    else if (p == FPM_READ_ERROR) {
        Serial.println("Got wrong PID or length!");
    } 
    else {
        Serial.println("Unknown error");
    }
}
