/**
 * ----------------------------------------------------------------------------
 * This implements the  MFRC522 library; see https://github.com/miguelbalboa/rfid
 * for further details and other examples.
 * 
 * NOTE: The library file MFRC522.h has a lot of useful info. Please read it.
 * 
 * Released into the public domain.
 * ----------------------------------------------------------------------------
 * This sample shows how to read and write data blocks on a MIFARE Classic PICC
 * (= card/tag).
 * 
 * BEWARE: Data will be written to the PICC, in sector #1 (blocks #4 to #7).
 * 
 * 
 * Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
 *             Reader/PCD   Uno           Mega      Nano v3    Leonardo/Micro   Pro Micro
 * Signal      Pin          Pin           Pin       Pin        Pin              Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
 * SPI SS      SDA(SS)      10            53        D10        10               10
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
 * SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
 * SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
 * 
 *
 * This is an interactive serial app, which will encode RFID cards for the Time's Tables
 * game
Use Sector 1, block 5 for command, block 6 for data

Block 5 - Command
00, FF, 00, A0 - Game mode (this card selects game type)
00, FF, 00, B0 - Playing card 

Block 6 - Data
10, 00, 00, 00 - Game Type - calculator
11, 00, 00, 00 - Game Type - tables test

00, 00, 00, 05 - Card data (in this case, final byte holds data)
 
 */

#include <SPI.h>
#include "MFRC522.h"

#define RST_PIN         9           // Configurable, see typical pin layout above
#define SS_PIN          10          // Configurable, see typical pin layout above


#define STATE_WAITING        0
#define STATE_READ_GAME_TYPE 1
#define STATE_READ_VALUE     2

#define COMMAND_CHANGE_GAME  'g'
#define COMMAND_PLAYING_CARD 'c'
#define COMMAND_EXAMINE_CARD 'e'
#define COMMAND_CALCULATOR   'c'
#define COMMAND_TEST         't'

#define COMMANDBYTE_CHANGE_GAME 0xA0 
#define COMMANDBYTE_PLAYING_CARD 0xB0

#define COMMANDBYTE_GAME_CALC 0x10
#define COMMANDBYTE_GAME_TEST 0x11
          
int current_state;    
char current_command;
byte commandByte;
byte typeByte;
byte valueByte;

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.

MFRC522::MIFARE_Key key;
/**
 * Initialize.
 */
void setup() {
    Serial.begin(115200); // Initialize serial communications with the PC
    while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
    SPI.begin();        // Init SPI bus
    mfrc522.PCD_Init(); // Init MFRC522 card

    // Prepare the key (used both as key A and as key B)
    // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }

    Serial.println(F("Scan a MIFARE Classic PICC to demonstrate read and write."));
    Serial.print(F("Using key (for A and B):"));
    dump_byte_array(key.keyByte, MFRC522::MF_KEY_SIZE);
    Serial.println();
    
    Serial.println(F("BEWARE: Data will be written to the PICC, in sector #1"));
    
    resetState();
}


/**
 * Main loop.
 */
void loop() 
{
   if(Serial.available())
   {
       byte b = Serial.read();
       switch(current_state)
       {
         case STATE_WAITING:
           current_command = b;
           switch(current_command)
           {
             case COMMAND_CHANGE_GAME:
               current_state = STATE_READ_GAME_TYPE;
               //print game options
               printGameOptions();
               break;
             case COMMAND_PLAYING_CARD:
               current_state = STATE_READ_VALUE;
               //print card options
               printValueOptions();
               break;
             case COMMAND_EXAMINE_CARD:
               examineCard();
               resetState();
             default:
               //error
               resetState();
           }
           break;
         case STATE_READ_GAME_TYPE:
          
          switch(b)
          {
            case COMMAND_CALCULATOR:
              writeData(COMMANDBYTE_CHANGE_GAME, COMMANDBYTE_GAME_CALC);
              resetState();
              break;
            case COMMAND_TEST:
              writeData(COMMANDBYTE_CHANGE_GAME, COMMANDBYTE_GAME_TEST);
              resetState();
              break;
            default:
              //print game options
               printGameOptions();             
              break;
          }
          break;
        case STATE_READ_VALUE:
          //b will be 'a'- 'l' for each doctor
          byte value = (b - 'a') + 1;
          Serial.print("Writing for Doctor number ");
          Serial.println(value);
          writeData(COMMANDBYTE_PLAYING_CARD, value);
          resetState();      
       }
   }  
}

void resetState()
{
  current_state = STATE_WAITING;
  
  //print instructions
  printInstructions();
}

void writeData(byte command, byte data)
{
    Serial.println("Present the card");
    // Look for new cards
    while(! mfrc522.PICC_IsNewCardPresent());
        

    // Select one of the cards
    while( ! mfrc522.PICC_ReadCardSerial());


    // Show some details of the PICC (that is: the tag/card)
    Serial.print(F("Card UID:"));
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println();
    Serial.print(F("PICC type: "));
    byte piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
    Serial.println(mfrc522.PICC_GetTypeName(piccType));

    // Check for compatibility
    if (    piccType != MFRC522::PICC_TYPE_MIFARE_MINI
        &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
        &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
        Serial.println(F("This sample only works with MIFARE Classic cards."));
        return;
    }

    // In this sample we use the second sector,
    // that is: sector #1, covering block #4 up to and including block #7
    byte sector         = 1;
    byte blockAddr      = 4;
    byte dataBlock[16]   = { 0x00, 0XFF, 0x00, 0x00, /* command */
                            0x00, 0x00, 0x00, 0x00, /* value */  
                            0x00, 0x00, 0x00, 0x00, /* padding */  
                            0x00, 0x00, 0x00, 0x00  /* padding */ };
    dataBlock[3] = command;
    if(command == COMMANDBYTE_CHANGE_GAME)
      dataBlock[4] = data;
    else if(command == COMMANDBYTE_PLAYING_CARD)
      dataBlock[7] = data;
                          
    byte trailerBlock   = 7;
    byte status;
    byte buffer[18];
    byte size = sizeof(buffer);

    // Authenticate using key A
    Serial.println(F("Authenticating using key A..."));
    status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("PCD_Authenticate() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        return;
    }

    // Show the whole sector as it currently is
    Serial.println(F("Current data in sector:"));
    mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
    Serial.println();

    // Read data from the block
    Serial.print(F("Reading data from block ")); Serial.print(blockAddr);
    Serial.println(F(" ..."));
    status = mfrc522.MIFARE_Read(blockAddr, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("MIFARE_Read() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
    }
    Serial.print(F("Data in block ")); Serial.print(blockAddr); Serial.println(F(":"));
    dump_byte_array(buffer, 16); Serial.println();
    Serial.println();

    // Authenticate using key B
    Serial.println(F("Authenticating again using key B..."));
    status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, trailerBlock, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("PCD_Authenticate() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        return;
    }

    // Write data to the block
    Serial.print(F("Writing data into block ")); Serial.print(blockAddr);
    Serial.println(F(" ..."));
    dump_byte_array(dataBlock, 16); Serial.println();
    status = mfrc522.MIFARE_Write(blockAddr, dataBlock, 16);
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("MIFARE_Write() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
    }
    Serial.println();

    // Read data from the block (again, should now be what we have written)
    Serial.print(F("Reading data from block ")); Serial.print(blockAddr);
    Serial.println(F(" ..."));
    status = mfrc522.MIFARE_Read(blockAddr, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("MIFARE_Read() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
    }
    Serial.print(F("Data in block ")); Serial.print(blockAddr); Serial.println(F(":"));
    dump_byte_array(buffer, 16); Serial.println();
        
    // Check that data in block is what we have written
    // by counting the number of bytes that are equal
    Serial.println(F("Checking result..."));
    byte count = 0;
    for (byte i = 0; i < 16; i++) {
        // Compare buffer (= what we've read) with dataBlock (= what we've written)
        if (buffer[i] == dataBlock[i])
            count++;
    }
    Serial.print(F("Number of bytes that match = ")); Serial.println(count);
    if (count == 16) {
        Serial.println(F("Success :-)"));
    } else {
        Serial.println(F("Failure, no match :-("));
        Serial.println(F("  perhaps the write didn't work properly..."));
    }
    Serial.println();
        
    // Dump the sector data
    Serial.println(F("Current data in sector:"));
    mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
    Serial.println();

    // Halt PICC
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
}

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}


void readBlock()
{
 // Look for new cards
    while(! mfrc522.PICC_IsNewCardPresent());
        

    // Select one of the cards
    while( ! mfrc522.PICC_ReadCardSerial());


    // Show some details of the PICC (that is: the tag/card)
    Serial.print(F("Card UID:"));
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println();
    Serial.print(F("PICC type: "));
    byte piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
    Serial.println(mfrc522.PICC_GetTypeName(piccType));

    // Check for compatibility
    if (    piccType != MFRC522::PICC_TYPE_MIFARE_MINI
        &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
        &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
        Serial.println(F("This sample only works with MIFARE Classic cards."));
    }

    // In this sample we use the second sector,
    // that is: sector #1, covering block #4 up to and including block #7
    byte sector         = 1;
    byte blockAddr      = 4;                        
    byte trailerBlock   = 7;
    byte status;
    byte buffer[18];
    byte size = sizeof(buffer);

    // Authenticate using key A
    Serial.println(F("Authenticating using key A..."));
    status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("PCD_Authenticate() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
    }

    // Show the whole sector as it currently is
    Serial.println(F("Current data in sector:"));
    mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
    Serial.println();

    // Read data from the block
    Serial.print(F("Reading data from block ")); Serial.print(blockAddr);
    Serial.println(F(" ..."));
    status = mfrc522.MIFARE_Read(blockAddr, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("MIFARE_Read() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
    }
    Serial.print(F("Data in block ")); Serial.print(blockAddr); Serial.println(F(":"));
    dump_byte_array(buffer, 16); Serial.println();
    Serial.println();
    
    commandByte = buffer[3];
    typeByte = buffer[4];
    valueByte = buffer[7];
    
    
    // Halt PICC
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
    
}

void examineCard()
{
  Serial.println("Present the card");
  readBlock();  
  
  Serial.println(commandByte);
  Serial.println(typeByte);
  Serial.println(valueByte);
  
  if(commandByte == COMMANDBYTE_CHANGE_GAME)
  {
    if(typeByte == COMMANDBYTE_GAME_CALC)
    {
      Serial.println("This changes the game to a calculator");
    }
    else if(typeByte == COMMANDBYTE_GAME_TEST)
    {
      Serial.println("This changes the game to a test");
    }
    else
    {
      Serial.println("Sorry, I don't recognise this card!");
    }
  }
  else if(commandByte == COMMANDBYTE_PLAYING_CARD)
  {
    Serial.print("This is for Doctor number ");
    Serial.println(valueByte);
  }
  else
  {
    Serial.println("Sorry, I don't recognise this card!");
  }
}


void printInstructions()
{
  Serial.println("Enter (c) to create a playing card, (g) to create a new game card,\n or (e) to examine an existing card");
}

void printGameOptions()
{  
  Serial.println("Enter (c) to create a calculator card, and (t) to create a new test card");
}

void printValueOptions()
{
  Serial.println("Enter the code for the Doctor who want");
  Serial.println("a - 1st Doctor (William Hartnell)");
  Serial.println("b - 2nd Doctor (Patrick Troughton)");
  Serial.println("c - 3rd Doctor (Jon Pertwee)");
  Serial.println("d - 4th Doctor (Tom Baker)");
  Serial.println("e - 5th Doctor (Peter Davison)");
  Serial.println("f - 6th Doctor (Colin Baker)");
  Serial.println("g - 7th Doctor (Sylvester McCoy)");
  Serial.println("h - 8th Doctor (Paul McGann)");
  Serial.println(" - there is no War Doctor - ");
  Serial.println("i - 9th Doctor (Christopher Eccleston)");
  Serial.println("j - 10th Doctor (David Tennent)");
  Serial.println("k - 11th Doctor (Matt Smith)");
  Serial.println("l - 12th Doctor (Peter Capaldi)");
}
