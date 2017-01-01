/*
*   It doesn't work for Teensy LC yet!
*/

#include "ADC.h"
#include "RingBufferDMA.h"

const int readPin = A9;

ADC *adc = new ADC(); // adc object

// Define the array that holds the conversions here.
// buffer_size must be a power of two.
// The buffer is stored with the correct alignment in the DMAMEM section
// the +0 in the aligned attribute is necessary b/c of a bug in gcc.
const int buffer_size = 512;
DMAMEM static volatile int16_t __attribute__((aligned(buffer_size+0))) buffer[buffer_size];

// use dma with ADC0
RingBufferDMA *dmaBuffer = new RingBufferDMA(buffer, buffer_size, ADC_0);

inline void toggleLED()
{
  digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN));
}

void configure1MhzPin3()
{
  //declaration
  unsigned int FTM1MODCount = 23;

  //initialise Flextimer 1
  FTM1_MODE = 0x05; //set write-protect disable (WPDIS) bit to modify other registers
  //FAULTIE=0, FAULTM=00, CAPTEST=0, PWMSYNC=0, WPDIS=1, INIT=0, FTMEN=1(no restriction FTM)
  FTM1_SC = 0x00; //set status/control to zero = disabled (enabled in main loop)
  FTM1_CNT = 0x0000; //reset count to zero
  FTM1_MOD = FTM1MODCount; //max modulus = 23 (gives count = 24 on roll-over - 1 MHz)
  FTM1_C0SC = 0x14; // CHF=0, CHIE=0 (disable interrupt), MSB=0 MSA=1, ELSB=0 ELSA=1 (output compare - toggle), 0, DMA=0
  //FTM1_C1SC = 0x14; // CHF=0, CHIE=0 (disable interrupt), MSB=0 MSA=1, ELSB=0 ELSA=1 (output compare - toggle), 0, DMA=0

  //configure Teensy pins as outputs
  PORTA_PCR12 |= 0x300; //MUX = alternative function 3 on Chip Pin 28 (FTM1_CH0) = Teensy Pin 3
  //PORTA_PCR13 |= 0x300; //MUX = alternative function 3 on Chip Pin 29 (FTM1_CH1) = Teensy Pin 4

  //enable system clock (48 MHz), no prescale
  FTM1_C0V = 0; //compare value = 0
  //FTM1_C1V = 12; //compare value = 12
  FTM1_SC = 0x08; // (Note - FTM0_SC [TOF=0 TOIE=0 CPWMS=0 CLKS=01 (System Clock 48 MHz) PS=000 [no prescale divide])

}

void setup() {


    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(readPin, INPUT); //pin 23 single ended


    Serial.begin(115200);

    ///// ADC0 ////
    // reference can be ADC_REF_3V3, ADC_REF_1V2 (not for Teensy LC) or ADC_REF_EXT.
    //adc->setReference(ADC_REF_1V2, ADC_0); // change all 3.3 to 1.2 if you change the reference to 1V2

    adc->setAveraging(1); // set number of averages
    adc->setResolution(12); // set bits of resolution

    // it can be ADC_VERY_LOW_SPEED, ADC_LOW_SPEED, ADC_MED_SPEED, ADC_HIGH_SPEED_16BITS, ADC_HIGH_SPEED or ADC_VERY_HIGH_SPEED
    // see the documentation for more information
    // additionally the conversion speed can also be ADC_ADACK_2_4, ADC_ADACK_4_0, ADC_ADACK_5_2 and ADC_ADACK_6_2,
    // where the numbers are the frequency of the ADC clock in MHz and are independent on the bus speed.
    adc->setConversionSpeed(ADC_MED_SPEED); // change the conversion speed
    // it can be ADC_VERY_LOW_SPEED, ADC_LOW_SPEED, ADC_MED_SPEED, ADC_HIGH_SPEED or ADC_VERY_HIGH_SPEED
    adc->setSamplingSpeed(ADC_MED_SPEED); // change the sampling speed

    // always call the compare functions after changing the resolution!
    //adc->enableCompare(1.0/3.3*adc->getMaxValue(ADC_0), 0, ADC_0); // measurement will be ready if value < 1.0V
    //adc->enableCompareRange(1.0*adc->getMaxValue(ADC_0)/3.3, 2.0*adc->getMaxValue(ADC_0)/3.3, 0, 1, ADC_0); // ready if value lies out of [1.0,2.0] V

    // enable DMA and interrupts
    adc->enableDMA(ADC_0);

    // If you enable interrupts, notice that the isr will read the result, so that isComplete() will return false (most of the time)
    // ADC interrupt enabled isn't mandatory for DMA to work.
    adc->enableInterrupts(ADC_0);

    Serial.println("Start DMA");
    dmaBuffer->start();

    for(int i=0; i<10; i++){
      toggleLED();
      delay(100);
    }

}

char c=0;

unsigned long ulStart(0);
void loop() {

     if (Serial.available()) {
      c = Serial.read();
      if(c=='c') { // start conversion
          Serial.println("Conversion: ");
          ulStart = micros();
          adc->startContinuous(readPin, ADC_0);
      } else if(c=='p') { // print buffer
          printBuffer();
      }else if(c=='r') { // print buffer
          resetBuffer();
      }
  }
}


// called everytime a new value is converted. The DMA isr is called first
voltile unsigned long ulData = {0};
void adc0_isr(void) {
    ADC0_RA; // clear interrupt
    if(dmaBuffer->isFull())
    {
      adc->stopContinuous();
      Serial.print("FULL -> ");
      Serial.print(dmaBuffer->b_size);
      Serial.print(" -> ");
      Serial.println(micros() - ulStart);
    }
}

void resetBuffer(){
    Serial.print("Clearing buffer...");
    while (!dmaBuffer->isEmpty())dmaBuffer->read();
    Serial.println("Done!");
}

void printBuffer() {
    Serial.println("Buffer: Address, Value");

    uint32_t i = 0;
    while (!dmaBuffer->isEmpty() ) {
        //Serial.print(uint32_t(&dmaBuffer->p_elems[i]), HEX);
        Serial.print(i++);Serial.print("\t");
        //Serial.println(dmaBuffer->p_elems[i]);
        Serial.println(dmaBuffer->read());
    }
}
