#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

// Filter and Time variables
unsigned long lastTime = 0;
float filteredPitch = 0.0;
float filteredRoll = 0.0;

// The Alpha tuning parameter (0.96 means trust gyro 96%, accel 4%)
const float ALPHA = 0.9;  //0.96

//Clicking feature
float previous_az = 0.0;
bool isFingerDown = false;
unsigned long lastStrikeTime  = 0;
const int DEBOUNCE_TIME = 50;

// Select I2C BUS (Renamed slightly to avoid confusion with the chip name)
void selectTCAChannel(uint8_t bus){
    Wire.beginTransmission(0x70); 
    Wire.write(1 << bus); 
    uint8_t error = Wire.endTransmission();
    
    // Error Detection: Check if multiplexer acknowledged the command
    if (error != 0) {
      Serial.println("I2C Error: Multiplexer not responding!");
      // Optional: Wire.begin(); // Attempt to reset the I2C bus
    }
}

float Clicking(float az){
    float delta_az = az - previous_az;
    previous_az = az;

    const float CLICK_STRIKE_THRESHOLD = -3.5;
    const float RELEASE_THRESHOLD = 2.0;

    if(delta_az < CLICK_STRIKE_THRESHOLD && !isFingerDown && (millis() - lastStrikeTime > DEBOUNCE_TIME)){
        isFingerDown = true;
        lastStrikeTime = millis();

        Serial.print(">>> CLICK PRESSED: ");
        Serial.print(delta_az);
        Serial.println(" <<<");

        delay(80);
    }
    else if(delta_az > RELEASE_THRESHOLD && isFingerDown && (millis() - lastStrikeTime > DEBOUNCE_TIME)){
        isFingerDown = false;
        lastStrikeTime = millis();

        Serial.println("--- Key Released --- ");
    }

    return delta_az;
}

void setup(){
    Serial.begin(115200); 
    Wire.begin();

    selectTCAChannel(7); // Ring finger bus
    
    if (!mpu.begin()){
        Serial.println("Failed to find Ring MPU6050. Check wiring!");
        while(1){ delay(10); }
    }
    Serial.println("Ring MPU6050 Found and Initialized.");
    
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G); // Lower range is better for finger tilts
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);   // Smooths out high-frequency vibrations
    
    lastTime = millis();
}

void loop(){
    selectTCAChannel(7);
    
    sensors_event_t a, g, temp;
    bool success = mpu.getEvent(&a, &g, &temp);

    // Error Detection: Ensure we actually got data
    if (!success || (a.acceleration.x == 0 && a.acceleration.y == 0 && a.acceleration.z == 0)) {
       Serial.println("Sensor read error or disconnected!");
       delay(100);
       return; // Skip this loop iteration
    }

    // 1. Calculate time elapsed (dt) in seconds
    unsigned long currentTime = millis();
    float dt = (currentTime - lastTime) / 1000.0; 
    lastTime = currentTime;

    // 2. Calculate Pitch and Roll from Accelerometer
    // Note: We use atan2 for a full 360-degree quadrant tracking
    float accelPitch = atan2(a.acceleration.y, sqrt(a.acceleration.x * a.acceleration.x + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;
    float accelRoll = atan2(-a.acceleration.x, a.acceleration.z) * 180.0 / PI;

    // 3. Get Gyro rates in degrees per second (Adafruit returns radians/s, so we convert)
    float gyroRateX = g.gyro.x * 180.0 / PI; // Roll rate
    float gyroRateY = g.gyro.y * 180.0 / PI; // Pitch rate

    // 4. Apply the Complementary Filter
    filteredPitch = ALPHA * (filteredPitch + gyroRateY * dt) + (1.0 - ALPHA) * accelPitch;
    filteredRoll = ALPHA * (filteredRoll + gyroRateX * dt) + (1.0 - ALPHA) * accelRoll;
    /*
    // Output for Serial Plotter
    Serial.print("Raw_Accel_Pitch:"); Serial.print(accelPitch); Serial.print(",");
    Serial.print("Filtered_Pitch:"); Serial.print(filteredPitch); Serial.print(",");
    Serial.print("Raw_Accel_Roll:"); Serial.print(accelRoll); Serial.print(",");
    Serial.print("Filtered_Roll:"); Serial.println(filteredRoll);// Writes \r\n
    */
    float Delta_Az = Clicking(a.acceleration.z);

    Serial.print(">"); 
    
    Serial.print("Delta_Az: ");
    Serial.print(Delta_Az);
    Serial.print(",");

    Serial.print("ax:");
    Serial.print(a.acceleration.x);
    Serial.print(",");
    
    Serial.print("ay:");
    Serial.print(a.acceleration.y);
    Serial.print(",");

    Serial.print("az:");
    Serial.print(a.acceleration.z);
    Serial.print(",");


    // 2. Variable:Value pairs separated by commas
    Serial.print("RawPitch:");
    Serial.print(accelPitch);
    Serial.print(",");

    Serial.print("FilteredPitch:");
    Serial.print(filteredPitch);
    Serial.print(",");

    Serial.print("RawRoll:");
    Serial.print(accelRoll);
    Serial.print(",");

    Serial.print("FilteredRoll:");
    Serial.print(filteredRoll);

    // 3. Mandatory line ending (\r\n)
    Serial.println(); 

    // Small delay to keep the loop at roughly 100Hz (10ms)
    delay(10);
}