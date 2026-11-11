/*
  74LVC574A & 74LVC245A 키패드 인터페이스 최종 통합 코드
  - Data Bus: GPIO 4, 5, 6, 7
  - 574 Clock: GPIO 13
  - 1차 245 OE (열 읽기): GPIO 10
  - 2차 245 OE (비활성화 대상): GPIO 14
  - RC 안정화 대기: 700us
  - C키(Row 2, Col 3) 미구현 반영 및 다중 샘플링 노이즈 필터링 적용
*/

const int PIN_OE_245_1 = 10;  // 첫 번째 74LVC245 Enable (Active Low)
const int PIN_OE_245_2 = 14;  // 두 번째 74LVC245 Enable (HIGH로 고정하여 비활성화)
const int PIN_CLK_574  = 13;  // 74LVC574A Clock
const int DATA_PINS[4] = {4, 5, 6, 7}; // 데이터 버스 핀

// 키맵 정의 (Row 2, Col 3의 C 위치는 구현되지 않아 '\0'으로 비워둠)
const char keyMap[4][4] = {
  {'1', '2', '3', 'A'}, 
  {'4', '5', '6', 'B'}, 
  {'7', '8', '9', '\0'}, // C키 미구현 영역
  {'*', '0', '#', 'D'}  
};

// 574 래치에 데이터 전송을 위한 클럭 펄스 인가 함수
void pulseClock() {
  digitalWrite(PIN_CLK_574, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_CLK_574, HIGH);
  delayMicroseconds(2);
  digitalWrite(PIN_CLK_574, LOW);
}

// 574 래치에 출력할 행(Row) 패턴 설정 함수
void setBusOutput(uint8_t nibble) {
  for (int i = 0; i < 4; i++) {
    pinMode(DATA_PINS[i], OUTPUT);
    digitalWrite(DATA_PINS[i], (nibble & (1 << i)) ? HIGH : LOW);
  }
}

// 245 버퍼를 통해 열(Column) 데이터를 읽기 위해 데이터 버스를 INPUT 모드로 전환
void inputMode() {
  for (int i = 0; i < 4; i++) {
    pinMode(DATA_PINS[i], INPUT);
  }
}

// 노이즈 필터: 지정된 핀을 여러 번 샘플링하여 모두 LOW인지 엄격하게 검증하는 함수
bool verifyPinLow(int pin, int samples, int delayUs) {
  for (int s = 0; s < samples; s++) {
    if (digitalRead(pin) != LOW) {
      return false; // 샘플링 중 하나라도 HIGH가 튀면 노이즈로 간주
    }
    if (s < samples - 1) {
      delayMicroseconds(delayUs);
    }
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Final Optimized Keypad Scan Started ===");

  // 핀 모드 설정
  pinMode(PIN_OE_245_1, OUTPUT);
  pinMode(PIN_OE_245_2, OUTPUT);
  pinMode(PIN_CLK_574, OUTPUT);

  // 1. 두 번째 74LVC245 칩은 충돌 방지를 위해 상시 비활성화 (HIGH)
  digitalWrite(PIN_OE_245_2, HIGH); 

  // 2. 첫 번째 74LVC245는 우선 차단 상태로 시작
  digitalWrite(PIN_OE_245_1, HIGH); 
  digitalWrite(PIN_CLK_574, LOW);

  inputMode();

  // 부팅 시 574 래치 초기화 (All HIGH로 잔류 전하 제거)
  setBusOutput(0x0F);
  pulseClock();
}

void loop() {
  for (int row = 0; row < 4; row++) {
    
    // STEP 1. 첫 번째 245 버퍼 차단 (버스 충돌 방지)
    digitalWrite(PIN_OE_245_1, HIGH); 
    delayMicroseconds(10); 

    // STEP 2. 선택된 행만 LOW로 574에 설정하고 클럭 인가
    uint8_t rowPattern = ~(1 << row) & 0x0F; 
    setBusOutput(rowPattern);
    pulseClock(); 
    
    // 💡 레거시 기판 RC 필터 방전/충전 특성을 고려한 700us 대기
    delayMicroseconds(700);

    // STEP 3. 버스를 INPUT 모드로 전환 (외부 10k 풀업 활용)
    inputMode();
    delayMicroseconds(100); 

    // STEP 4. 첫 번째 245 활성화 (열 상태 읽기)
    digitalWrite(PIN_OE_245_1, LOW); 
    delayMicroseconds(20); 

    // 열(Column) 스캔
    for (int col = 0; col < 4; col++) {
      if (digitalRead(DATA_PINS[col]) == LOW) {
        
        // 다중 샘플링 필터 적용 (100us 간격으로 3회 연속 LOW 검증)
        if (verifyPinLow(DATA_PINS[col], 1, 100)) {
          
          // C키가 없는 위치(Row 2, Col 3)인 경우 감지되어도 무시
          if (row == 2 && col == 3) {
            continue;
          }
          
          Serial.print("Row ");
          Serial.print(row);
          Serial.print(" Active -> Col ");
          Serial.print(col);
          Serial.print(" Detected! | Pressed Key: ");
          Serial.println(keyMap[row][col]);
          
          // 키 릴리즈 대기 (손을 뗄 때 접점 바운스 방지)
          while (true) {
            delay(10);
            bool released = true;
            for (int s = 0; s < 3; s++) {
              if (digitalRead(DATA_PINS[col]) == LOW) {
                released = false;
                break;
              }
              delayMicroseconds(50);
            }
            if (released) break;
          }
        }
      }
    }

    // STEP 5. 첫 번째 245 차단
    digitalWrite(PIN_OE_245_1, HIGH);

    // STEP 6. 574 래치를 All HIGH(0x0F)로 리셋하여 잔류 전하 소멸
    setBusOutput(0x0F);
    pulseClock();
    
    delay(10); // 행간 스캔 간격
  }
}
