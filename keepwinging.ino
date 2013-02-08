#include <SoftwareSerial.h>

int ring_totals = 4;
int rings[4] = {11, 10, 6, 5};
int ring_value[4] = {0, 0, 0, 255};
int ring_direction[4] = {1, 1, 1, 1};

SoftwareSerial RFID(3, 13);

int kticks = 0;
int ticks = 0;

/**
 * Who owns the current effect
 * 0 = Nobody
 * 1 = Ping failure
 * 2 = Card send in progress
 * 3 = Card send failue
 * 4 = Card sent
 */
int effect_owner = 0;

/**
 * Contains the ping status.
 * 0 == Waiting for pong. If 10 kticks pass without pong, panic.
 * 1 == Pong received but not acknowleged.
 * 2 == Pong received, pong acknowleged.
 */
int host_ponged = 1;

/**
 * Record the number of ticks since the card was sent to the host
 */
int card_send_ticks = 0;

/**
 * If the machine is in a panic state regarding the card receipt.
 */
int card_send_panic = 0;

/**
 * Contains the status of if a card has been received.
 * 0 == No card has been received.
 * 1 == Card received and waiting to be sent to host.
 */
int card_recvd = 0;

/**
 * If the host received the last card.
 * 0 == No.
 * 1 == Yes.
 */
int host_recvd = 0;

/**
 * Ticks since card sent
 */
int card_sent_ticks = 0;

void (*effect_tick)(void);

void setup()
{
  RFID.begin(9600);
  Serial.begin(9600);


  pinMode(13, INPUT); // For test button

  for (int i = 0; i < ring_totals; i++) {
    pinMode(rings[i], OUTPUT);
  }
  
  notify_initialized();


}

void loop()
{
  srl_check(Serial);
  host_ping();
  
  // If the host received the last card, reset to normal state.
  if (host_recvd == 1) {
    host_recvd = 0;
    card_recvd = 0;
    card_send_ticks = 0;
    card_send_panic = 0;
    notify_card_sent();
  } else if (card_recvd == 1 && card_send_panic == 0) {
    card_send_ticks++;
  } else {
    rfid_read();
  }
  
  if (card_sent_ticks >= 0) {
    card_sent_ticks++;
  }
  
  if (card_sent_ticks >= 0 && card_sent_ticks > 1000) {
    card_sent_ticks = -1;
    effect_owner = 0;
    effect_pulse_prep_in();
  }
  
  if (card_send_ticks > 1000 && card_send_panic == 0) {
    card_send_panic = 1;
    notify_card_send_failure();
  }

  if (effect_tick) {
    effect_tick();
  }

  tick_update();
  delay(1);
}

void srl_check(Stream &srl)
{
  char data[20];
  byte index = 0;
  
  for (int i = 0; i < 20; i++) {
    data[i] = 0;
  }
  
  // ensure the entire message is available before reading
  if (srl.available() < 7) {
    return;
  }
  
  for (int i = 0; i < 20 && srl.available() > 0; i++) {
    data[i] = srl.read();
  }

  if (0 == strcmp(data,        "PINGED\n")) {
    Serial.println("DBG: Pong received.");
    host_ponged = 1;
  } else if (0 == strcmp(data, "CRECVD\n")) {
    Serial.println("DBG: Card reciept.");
    host_recvd = 1;
  }    
}


void tick_update()
{
  ticks++;
  
  if (ticks == 1000) {
    kticks++;
    ticks = 0;
  }

  if (kticks == 20) {
    kticks = 0;
  }
}



void host_ping()
{
  if (host_ponged == 1) {
    host_ponged = 2;
    notify_ping_success();
  }
  
  // Execute on the first and every 10k ticks
  if (ticks == 0 && (kticks % 2 == 0)) {
    if (host_ponged == 0) {
      notify_ping_failure();
    }

    host_ponged = 0;
    Serial.println("PING: You there?");
  }
}

void notify_initialized()
{
  Serial.println("WINGIT: 2013");
  effect_pulse_prep_out();
  effect_owner = 0;
}

void notify_ping_success()
{
  if (effect_owner == 1 || effect_owner == 0) {
    Serial.print("NOTIFY: Ping success: ");
    Serial.println(effect_owner);
    effect_owner = 0;
    effect_pulse_prep_in();
  }
}

void notify_ping_failure()
{
  effect_owner = 1;
  effect_blink_prep_cross();
  
  Serial.println("NOTIFY: Ping failure. Forced.");
}

void notify_card_send_failure()
{
  if (effect_owner == 0 || effect_owner == 2) {
    Serial.print("NOTIFY: Card send failure: ");
    Serial.println(effect_owner);
    effect_owner = 3;
    effect_blink_prep_straight();
  }
}

void notify_card_sending()
{
  if (effect_owner == 0 || effect_owner == 3) {
    Serial.print("NOTIFY: Card sending: ");
    Serial.println(effect_owner);

    effect_owner = 2;
    effect_pulse_prep_straight();
  }
}

void notify_card_sent()
{
  if (effect_owner == 0 || effect_owner == 2 || effect_owner == 3) {
    effect_blink_prep_straight();
    effect_owner = 4;
    card_sent_ticks = 0;
  }
}


void effect_blink_prep_cross()
{
  effect_tick = &effect_blink_tick;
  for (int i = 0; i < ring_totals; i++) {
    ring_value[i] = 0;
    ring_direction[i] = (i % 2 == 0);
  }
}

void effect_blink_prep_straight()
{
    effect_tick = &effect_blink_tick;
    
    for (int i = 0; i < ring_totals; i++) {
    ring_value[i] = 0;
    ring_direction[i] = 0;
  }
}

void effect_blink_tick()
{
  if (ticks % 100 == 0) {
    int cur_dir;
    
    for (int i = 0; i < ring_totals; i++) {
      cur_dir = ring_direction[i];
      
      ring_value[i] = cur_dir * 255;
      ring_direction[i] = (cur_dir == 0);
    }
    writeOutRings();

  }
}


void effect_pulse_prep_in()
{
  effect_pulse_prep(1);
}

void effect_pulse_prep_out()
{
  effect_pulse_prep(-1);
}

void effect_pulse_prep_straight()
{
  effect_tick = &effect_pulse_tick;

  for (int i = 0; i < ring_totals; i++) {
    ring_value[i] = 0;
    ring_direction[i] = 1;
  }
}

void effect_pulse_prep(int dir)
{
  effect_tick = &effect_pulse_tick;
  
  int steps = 255 / ring_totals;
  for (int i = 0; i < ring_totals; i++) {
    ring_value[i] = steps * i;
    ring_direction[i] = dir;
  }
}

void effect_pulse_tick()
{
  for (int i = 0; i < ring_totals; i++) {
    if (ring_value[i] >= 255) {
      ring_direction[i] = -1;
    } else if (ring_value[i] <= 0) {
      ring_direction[i] = 1;
    }
    
    ring_value[i] += ring_direction[i];
  }
  
  writeOutRings(); 
}

void writeOutRings()
{
  for (int i = 0; i < ring_totals; i++) {
    analogWrite(rings[i], ring_value[i]);
  }
}




void rfid_read() {
    byte i = 0;
    byte val = 0;
    byte code[6];
    byte checksum = 0;
    byte bytesread = 0;
    byte tempbyte = 0;
    
    if ((val = RFID.read()) == 2) {
        // read 10 digit code + 2 digit checksum
        while (bytesread < 12) {
            if (RFID.available() > 0) {
                val = RFID.read();
            
                // if header or stop bytes before the 10 digit reading, stop reading.
                if ((val == 0x0D) || (val == 0x0A) || (val == 0x03) || (val == 0x02)) {
                    break;
                }
            
                // Do Ascii/Hex conversion:
                if ((val >= '0') && (val <= '9')) {
                    val = val - '0';
                } else if ((val >= 'A') && (val <= 'F')) {
                    val = 10 + val - 'A';
                }
            
                // Every two hex-digits, add byte to code:
                if (bytesread & 1 == 1) {
                    // make some space for this hex-digit by
                    // shifting the previous hex-digit with 4 bits to the left:
                    code[bytesread >> 1] = (val | (tempbyte << 4));

                    // If we're at the checksum byte, calculate the checksum (XOR)
                    if (bytesread >> 1 != 5) {
                        checksum ^= code[bytesread >> 1];
                    }
                } else {
                    // Store the first hex digit first...
                    tempbyte = val;
                }
            
                // ready to read next digit
                bytesread++;
            }
        }
    }
    
    
    if (bytesread == 12) {
        if (code[5] != checksum) {
          return;
        } else {
          Serial.print("CARD:");
           for (int j = 0; j < 5; j++) {
              if (code[j] < 16) Serial.print("0"); 
              Serial.print(code[j], HEX);
           }
           Serial.println();
        }
        
        card_recvd = 1;
        notify_card_sending();
        return;
    }
    
    bytesread = 0;
    
    return;
}
