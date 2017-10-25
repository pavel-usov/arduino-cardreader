#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 9
#define SS_PIN  10

#define SERIAL_TIMEOUT 20000
#define SERIAL_BUF_LEN 64
#define RFID_PICC_TIMEOUT 1000
#define RFID_PICC_BLK_LEN 16
#define RFID_READ_BUF_LEN 18

typedef struct {
  MFRC522::MIFARE_Key key;
  byte type;
  byte block;
} PICC_AUTH_DATA;

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  while (!Serial);
  SPI.begin();
  mfrc522.PCD_Init();
  press_enter();
}

void loop() {
  char msg[RFID_PICC_BLK_LEN];
    
  if(!serial_read_str(msg, RFID_PICC_BLK_LEN)) return;
  if(!rfid_write_str(msg, RFID_PICC_BLK_LEN)) return;
  Serial.println(F("Success!"));
}

// Functions for RFID

bool picc_get_id(MFRC522::Uid *uid) {
  unsigned int iter = 0;

  mfrc522.PCD_StopCrypto1();
  while(iter < RFID_PICC_TIMEOUT) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      memcpy(uid, &mfrc522.uid, sizeof(MFRC522::Uid));
      return true;
    }
    iter ++;
    delay(1);
  }
  return false;
}

byte picc_write_block(MFRC522::Uid *uid, PICC_AUTH_DATA *key, byte *data) {
  MFRC522::StatusCode status;
  byte buf_size = RFID_READ_BUF_LEN, data_buf[RFID_READ_BUF_LEN];
  
  if (key->block == 0 || key->block%4 == 3) return MFRC522::STATUS_INVALID;  // Blocks may not be writable
  if ((status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(key->type, key->block/4, &key->key, uid)) == MFRC522::STATUS_OK) {
    if ((status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(key->block, data, RFID_PICC_BLK_LEN)) == MFRC522::STATUS_OK) {
      if ((status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(key->block, data_buf, &buf_size)) == MFRC522::STATUS_OK) {
        if (memcmp(data, data_buf, RFID_PICC_BLK_LEN) != 0) status = MFRC522::STATUS_ERROR;
      }
    }
  }
  return status;
}

bool rfid_write_str(char *str, unsigned char len) {
  MFRC522::Uid picc_id;
  MFRC522::StatusCode status;
  PICC_AUTH_DATA picc_key;
  byte data[RFID_PICC_BLK_LEN];

  if (len > RFID_PICC_BLK_LEN) {
    Serial.println(F("String is too long to fit in a single block on the card"));
    return false;
  }
  picc_key.key = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  picc_key.type = MFRC522::PICC_CMD_MF_AUTH_KEY_A;
  picc_key.block = 1;
  memcpy(data, str, len);
  
  Serial.println(F("Please place a card on the rfid reader/writer"));
  if (!picc_get_id(&picc_id)) {
    Serial.println(F("No card was found. Aborting"));
    return false;
  }
  if ((status = (MFRC522::StatusCode)picc_write_block(&picc_id, &picc_key, data)) != MFRC522::STATUS_OK) {
    Serial.print(F("PICC write failed: "));
    Serial.print(mfrc522.GetStatusCodeName(status));
    Serial.println(F(" Aborting"));
    return false;
  }
  return true;  
}

// Functions for reading from serial interface

bool read_serial(byte *in) {
  unsigned int iter = 0;
  
  while (iter < SERIAL_TIMEOUT) {
    if (Serial.available()) {
      *in = Serial.read();
      return true;
    }
    iter ++;
    delay(1);
  }
  return false;
}

void reprint_string() {
  char ansi_back[4] = { 0x1b, 0x5b, 0x44, 0x00 };
  Serial.print(ansi_back);
  Serial.print(F(" "));
  Serial.print(ansi_back);
}

unsigned char readln_serial(char *in_buf, unsigned char buf_len) {
  byte in;
  unsigned char pos = 0;
  
  while (pos < buf_len) {
    if (!read_serial(&in)) return 0;
    switch (in) {
      case 8:
        if (pos > 0) pos --;
        in_buf[pos] = 0;
        reprint_string();
        break;
      case 13:
        in_buf[pos] = 0;
        Serial.println();
        return ++ pos;
      default:
        in_buf[pos] = in;
        Serial.write(in);
        pos ++;
    }
  }
  Serial.println();
  return ++ pos;
}

void press_enter() {
  byte in_buf = 0;
  
  while (in_buf != 13) {
    read_serial(&in_buf);
  }
}

// Functions for reading string data type

bool serial_read_str(char *str, unsigned char len) {
  char in_buf[SERIAL_BUF_LEN];
  unsigned char in_bytes;
  
  Serial.println(F("Please enter a string you want to store"));
  if (!(in_bytes = readln_serial(in_buf, SERIAL_BUF_LEN))) {
    Serial.println(F("\nInput timeout"));
    return false;
  }
  if(in_bytes > len) {
    Serial.println(F("String is too long"));
    return false;
  }
  memcpy(str, in_buf, in_bytes);
  return true;
}
