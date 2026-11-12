// 사용자 지정 핀 정의
const int PIN_OE  = 10;                  // J1-12번 (U2 Output Enable)
const int PIN_CP  = 11;                  // J1-13번 (74HCT574 Clock)
const int SW_PINS[5] = {4, 5, 6, 7, 8};  // J1-4, 5, 6, 7, 8번 (SW1 ~ SW5 / D0 ~ D4)

int lastButtonState[5] = {HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounceTime[5] = {0, 0, 0, 0, 0};
const unsigned long debounceDelay = 30;  // 채터링 방지 시간 (ms)

void setup() {
  Serial.begin(115200);

  // 1. U2(74LVC245) 출력 활성화 (스위치 읽기 모드)
  pinMode(PIN_OE, OUTPUT);
  digitalWrite(PIN_OE, LOW);

  // 2. 74HCT574 클럭 핀 초기화
  pinMode(PIN_CP, OUTPUT);
  digitalWrite(PIN_CP, LOW);

  // 3. 스위치 핀을 입력 모드로 설정
  for (int i = 0; i < 5; i++) {
    pinMode(SW_PINS[i], INPUT);
  }

  Serial.println("시스템 준비 완료: 버튼을 누르면 74HCT574에 래치됩니다.");
}

// 74HCT574에 특정 비트 패턴(active-high)을 기록하는 함수
void writeLatch(uint8_t value) {
  // 1) 데이터 버스 충돌 방지를 위해 U2(74LVC245) 출력 비활성화 (High-Z)
  digitalWrite(PIN_OE, HIGH);

  // 2) ESP32 핀들을 출력으로 전환하고 원하는 값(눌린 버튼만 1) 세팅
  for (int i = 0; i < 5; i++) {
    pinMode(SW_PINS[i], OUTPUT);
    digitalWrite(SW_PINS[i], (value >> i) & 0x01);
  }

  // 3) 74HCT574 Clock(GPIO 11) 상승 엣지(Rising Edge) 발생
  delayMicroseconds(5);
  digitalWrite(PIN_CP, HIGH);
  delayMicroseconds(5);
  digitalWrite(PIN_CP, LOW);

  // 4) 다음 스위치 입력을 받기 위해 핀을 다시 입력으로 복구하고 U2 활성화
  for (int i = 0; i < 5; i++) {
    pinMode(SW_PINS[i], INPUT);
  }
  digitalWrite(PIN_OE, LOW);
}

void loop() {
  for (int i = 0; i < 5; i++) {
    int reading = digitalRead(SW_PINS[i]);

    // 상태 변화 감지 시 디바운스 타이머 리셋
    if (reading != lastButtonState[i]) {
      lastDebounceTime[i] = millis();
    }

    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      static bool pressedState[5] = {false, false, false, false, false};

      // 버튼이 눌렸을 때 (Active-Low -> LOW 감지)
      if (reading == LOW && !pressedState[i]) {
        pressedState[i] = true;

        // 현재 눌린 스위치(i)에 해당하는 비트만 1로 설정 (이전 값은 자동 덮어쓰기됨)
        uint8_t latchData = (1 << i);
        writeLatch(latchData);

        Serial.print("SW");
        Serial.print(i + 1);
        Serial.print(" 눌림 -> 74HCT574 래치 출력 값: 0b");
        Serial.println(latchData, BIN);
      } 
      else if (reading == HIGH) {
        pressedState[i] = false;
      }
    }

    lastButtonState[i] = reading;
  }
}
