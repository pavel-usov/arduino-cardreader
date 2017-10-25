#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 9
#define SS_PIN  10

#define RFID_PICC_BLK_LEN 16
#define RFID_READ_BUF_LEN 18
#define RFID_PICC_TIMEOUT 1000

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
}

void loop() {
  char msg[RFID_PICC_BLK_LEN];
  
  if (!rfid_read_str(msg, RFID_PICC_BLK_LEN)) return;
  Serial.print(F("Got: "));
  Serial.println(msg);
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

byte picc_read_block(MFRC522::Uid *uid, PICC_AUTH_DATA *key, byte *data) {
  MFRC522::StatusCode status;
  byte buf_size = RFID_READ_BUF_LEN;
  
  if ((status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(key->type, key->block/4, &key->key, uid)) == MFRC522::STATUS_OK)
    status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(key->block, data, &buf_size);
  return status;
}

bool rfid_read_str(char *str, unsigned char len) {
  MFRC522::Uid picc_id;
  MFRC522::StatusCode status;
  PICC_AUTH_DATA picc_key;
  byte data[RFID_READ_BUF_LEN];
  
  picc_key.key = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  picc_key.type = MFRC522::PICC_CMD_MF_AUTH_KEY_A;
  picc_key.block = 1;
  
  Serial.println(F("Please place a card on the rfid reader"));
  if(!picc_get_id(&picc_id)) {
    Serial.println(F("No card was found. Aborting"));
    return false;
  }
  if ((status = (MFRC522::StatusCode)picc_read_block(&picc_id, &picc_key, data)) != MFRC522::STATUS_OK) {
    Serial.print(F("PICC read failed: "));
    Serial.print(mfrc522.GetStatusCodeName(status));
    Serial.println(F(" Aborting"));
    return false;
  }
  strncpy(str, (char*) data, len);
  mfrc522.PICC_HaltA();
  return true;
}

