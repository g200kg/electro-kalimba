/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <math.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

DAC_HandleTypeDef hdac1;
DMA_HandleTypeDef hdma_dac1_ch1;

TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;
TIM_HandleTypeDef htim17;
DMA_HandleTypeDef hdma_tim17_ch1_up;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM17_Init(void);
static void MX_ADC1_Init(void);
static void MX_DAC1_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM7_Init(void);
/* USER CODE BEGIN PFP */


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//###### DAC1 Out1 ######
// Use DAC1-Out1 for audio output
//  Driven by DMA1-Ch3 trig TIM6 DMA Circular

#define OUTBUFFLEN (2048)
#define OUTBUFFLENHALF (OUTBUFFLEN / 2)

uint16_t outBuff[OUTBUFFLEN];		// OutputBuffer for DAC
int outBuffAvail0 = 1;				// Buffer First half ready
int outBuffAvail1 = 1;				// Buffer Latter half ready


//###### ADC1 Ch1,Ch2,Ch4 ######
// Read 3 pots analog value ADC1-Ch1, ADC1-Ch2, ADC-Ch4 by DMA1-Ch1 Circular

uint16_t adcVal[3] = {
		0, 0, 0,
};

//###### NeoPixel ######
// NeoPixel LED Driven by TIM17-Ch1 PWM, duty controlled by DMA1-Ch7 (neoPixelBuff)
//
#define NEOPIXELNUM 4
#define NEOPIXELPREAMBLE 50
#define NEOPIXELPOSTAMBLE 10
#define NEOPIXELBUFFLEN (24 * NEOPIXELNUM + NEOPIXELPREAMBLE + NEOPIXELPOSTAMBLE)

// NeoPixel duty definition
#define NEOPIXBIT0	12
#define NEOPIXBIT1	26
int tim17prescaler = 1;

// PWM duty control value array
uint8_t neoPixelBuff[NEOPIXELBUFFLEN] = {
		0,0,0,0,0,0,0,0,0,0,	// preamble
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,

		0,0,0,0,0,0,0,0,0,0,	// LED 0
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,

		0,0,0,0,0,0,0,0,0,0,	// LED 1
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,

		0,0,0,0,0,0,0,0,0,0,	// LED 2
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,

		0,0,0,0,0,0,0,0,0,0,	// LED 3
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,

		0,0,0,0,0,0,0,0,0,0,	// postamble
};
uint8_t neoPixelBuffNone[16] = {
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
};

//###### Keys #####
#define VOICEMAX 4

int keyActive[VOICEMAX];
int keyActiveQue[VOICEMAX];

// KO 0-2 = PA12, PA8, PA11
// KI 0-5 = GPIO PortB bit => 5432xx10
int keyOut, keyInBits, keyInBitsOld;

// Read KI 0-5 bits
// EK002 PB 5432xx10
#define KEYINBITS ((GPIOB->IDR & 0b11) | ((GPIOB->IDR >> 2) & 0b00111100))

// KO 0-2 port & pin
//EK002
#define KEYOUT0_PORT GPIOA
#define KEYOUT1_PORT GPIOA
#define KEYOUT2_PORT GPIOA
#define KEYOUT0_PIN GPIO_PIN_12
#define KEYOUT1_PIN GPIO_PIN_8
#define KEYOUT2_PIN GPIO_PIN_11


//###### Osc ######

#define OSCFSORIG (24000)
int oscFsTune = OSCFSORIG;
int oscDelta[17];
int oscPhaseActive[VOICEMAX];
int envPhaseActive[VOICEMAX];
int oscMix1, oscMix2;
int envPhaseRate;

// Buzz Osc
int modPhase = 0;
int modDelta = 2555;
int modType = 0;

// NeoPixel status
int ledOut[3];

// Frequencies for each keys
#define OCTAVE 3

int oscFreq[17] = {
		(int)(1174.6590716696303 * (0x02000 << OCTAVE)),	//D5
		(int)(987.7666025122483  * (0x02000 << OCTAVE)),	//B4
		(int)(783.9908719634986  * (0x02000 << OCTAVE)), 	//G4
		(int)(659.2551138257398  * (0x02000 << OCTAVE)),	//E4
		(int)(523.2511306011972  * (0x02000 << OCTAVE)),	//C4
		(int)(440.0              * (0x02000 << OCTAVE)),	//A3
		(int)(349.2282314330039  * (0x02000 << OCTAVE)),	//F3
		(int)(293.6647679174076  * (0x02000 << OCTAVE)),	//D3
		(int)(261.6255653005986  * (0x02000 << OCTAVE)),	//C3
		(int)(329.6275569128699  * (0x02000 << OCTAVE)),	//E3
		(int)(391.99543598174927 * (0x02000 << OCTAVE)),	//G3
		(int)(493.8833012561241  * (0x02000 << OCTAVE)),	//B3
		(int)(587.3295358348151  * (0x02000 << OCTAVE)),	//D4
		(int)(698.4564628660078  * (0x02000 << OCTAVE)),	//F4
		(int)(880.0              * (0x02000 << OCTAVE)),	//A4
		(int)(1046.5022612023945 * (0x02000 << OCTAVE)),	//C5
		(int)(1318.5102276514797 * (0x02000 << OCTAVE)), 	//E5
};

// Waveforms

/*
int waveTabA[] = {
		0,
		195,
		383,
		556,
		707,
		831,
		924,
		981,
		1000,
		981,
		924,
		831,
		707,
		556,
		383,
		195,
		0,
		-195,
		-383,
		-556,
		-707,
		-831,
		-924,
		-981,
		-1000,
		-981,
		-924,
		-831,
		-707,
		-556,
		-383,
		-195,
		0,
};
int waveTabB[] = {
		0,
		24,
		125,
		212,
		220,
		260,
		378,
		446,
		434,
		498,
		644,
		676,
		620,
		752,
		991,
		800,
		0,
		-800,
		-991,
		-752,
		-620,
		-676,
		-644,
		-498,
		-434,
		-446,
		-378,
		-260,
		-220,
		-212,
		-125,
		-24,
		0,
};

int waveTabC[] = {
		0,
		835,
		1007,
		400,
		-471,
		-928,
		-706,
		-150,
		133,
		-150,
		-706,
		-928,
		-471,
		400,
		1007,
		835,
		0,
		-835,
		-1007,
		-400,
		471,
		928,
		706,
		150,
		-133,
		150,
		706,
		928,
		471,
		-400,
		-1007,
		-835,
		0,

};


int waveTabC[] = {
		0,
		941,
		672,
		-342,
		-964,
		-872,
		-460,
		-120,
		0,
		-120,
		-460,
		-872,
		-964,
		-342,
		672,
		941,
		-941,
		-672,
		342,
		964,
		872,
		460,
		120,
		0,
		120,
		460,
		872,
		964,
		342,
		-672,
		-941,
		0,
		0,
};
*/

int waveTabA[] = {
		0,
		98,
		195,
		290,
		383,
		471,
		556,
		634,
		707,
		773,
		831,
		882,
		924,
		957,
		981,
		995,
		1000,
		995,
		981,
		957,
		924,
		882,
		831,
		773,
		707,
		634,
		556,
		471,
		383,
		290,
		195,
		98,
		0,
		-98,
		-195,
		-290,
		-383,
		-471,
		-556,
		-634,
		-707,
		-773,
		-831,
		-882,
		-924,
		-957,
		-981,
		-995,
		-1000,
		-995,
		-981,
		-957,
		-924,
		-882,
		-831,
		-773,
		-707,
		-634,
		-556,
		-471,
		-383,
		-290,
		-195,
		-98,
		0,
};
int waveTabB[] = {
		0,
		3,
		24,
		67,
		125,
		179,
		212,
		221,
		220,
		228,
		260,
		315,
		378,
		426,
		446,
		442,
		434,
		449,
		498,
		573,
		644,
		682,
		676,
		642,
		620,
		652,
		752,
		888,
		991,
		980,
		800,
		452,
		0,
		-452,
		-800,
		-980,
		-991,
		-888,
		-752,
		-652,
		-620,
		-642,
		-676,
		-682,
		-644,
		-573,
		-498,
		-449,
		-434,
		-442,
		-446,
		-426,
		-378,
		-315,
		-260,
		-228,
		-220,
		-221,
		-212,
		-179,
		-125,
		-67,
		-24,
		-3,
		0,
};
int waveTabC[] = {
		0,
		523,
		897,
		1029,
		915,
		634,
		315,
		81,
		0,
		66,
		210,
		339,
		379,
		312,
		178,
		52,
		0,
		52,
		178,
		312,
		379,
		339,
		210,
		66,
		0,
		81,
		315,
		634,
		915,
		1029,
		897,
		523,
		0,
		-523,
		-897,
		-1029,
		-915,
		-634,
		-315,
		-81,
		0,
		-66,
		-210,
		-339,
		-379,
		-312,
		-178,
		-52,
		0,
		-52,
		-178,
		-312,
		-379,
		-339,
		-210,
		-66,
		0,
		-81,
		-315,
		-634,
		-915,
		-1029,
		-897,
		-523,
		0,
};
int waveTabD[] = {
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		-700,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
};
// Current waveform (The output will be mix of waveTab1 and waveTab2)
int *waveTab1 = waveTabA;
int *waveTab2 = waveTabB;

// Envelope curve
int envTab[] = {
		1024,
		870,
		740,
		629,
		535,
		454,
		386,
		328,
		279,
		237,
		202,
		171,
		146,
		124,
		105,
		89,
		76,
		65,
		55,
		47,
		40,
		34,
		29,
		24,
		21,
		18,
		15,
		13,
		11,
		9,
		8,
		7,
		6,
		5,
		4,
		3,
		3,
		3,
		2,
		0
};
#define ENVTABLEN (sizeof envTab / sizeof envTab[0])
#define ENVPHASEMAX ((ENVTABLEN -1) << 20)

int key2LedOut[] = {
		0,
		0,
		0,
		1,
		1,
		1,
		2,
		2,
		2,
		2,
		2,
		1,
		1,
		1,
		0,
		0,
		0,
};

// Select the least important voice
int disposeVoice(int key) {
	int v = 0;
	int envMax = 0;
	for(int i=0; i < VOICEMAX; ++i) {
		if(envPhaseActive[i] >= ENVPHASEMAX)
			return i;
		if(keyActive[i] == key)
			return i;
		if(envPhaseActive[i] > envMax) {
			envMax = envPhaseActive[i];
			v = i;
		}
	}
	return v;
}
// Assign new voice
void assignVoice(int key) {
//	printf("key: %d\r\n",key);
	int voice = disposeVoice(key);
	if(envPhaseActive[voice] < ENVPHASEMAX) {
		keyActiveQue[voice] = key;
	}
	else {
		oscPhaseActive[voice] = 0;
		keyActive[voice] = key;
		envPhaseActive[voice] = 0;
	}
}
// Setup NeoPix N to specified BRG color
void neoPixelSetCol(int n, int brg) {
	int offset = NEOPIXELPREAMBLE + (n * 24);
	for(int i = 0; i < 24; ++i, brg <<= 1) {
		if(brg & 0x800000)
			neoPixelBuff[offset + i] = NEOPIXBIT1;
		else
			neoPixelBuff[offset + i] = NEOPIXBIT0;
	}
}
// Scan keys
void scanKeys() {
	keyInBits = (keyInBits << 6) | KEYINBITS;						// collect key status
	neoPixelSetCol(keyOut, (keyInBits & 0b111111) ? 0x000010:0);
	if(--keyOut < 0) {
		int xor = (keyInBits ^ keyInBitsOld) & keyInBits;			// xor : keyin rising edge for all keys
		for(int n = 0;n < 17; ++n, xor >>= 1) {
			if(xor & 1) {
				assignVoice(n);
			}
		}
		keyOut = 2;
		keyInBitsOld = keyInBits;
		keyInBits = 0;
	}
    switch(keyOut) {												// next scan (KO2 => KO1 => KO0)
    case 0:
    	HAL_GPIO_WritePin(KEYOUT1_PORT, KEYOUT1_PIN, GPIO_PIN_RESET);
    	HAL_GPIO_WritePin(KEYOUT0_PORT, KEYOUT0_PIN, GPIO_PIN_SET);
    	break;
    case 1:
    	HAL_GPIO_WritePin(KEYOUT2_PORT, KEYOUT2_PIN, GPIO_PIN_RESET);
    	HAL_GPIO_WritePin(KEYOUT1_PORT, KEYOUT1_PIN, GPIO_PIN_SET);
    	break;
    case 2:
    	HAL_GPIO_WritePin(KEYOUT0_PORT, KEYOUT0_PIN, GPIO_PIN_RESET);
    	HAL_GPIO_WritePin(KEYOUT2_PORT, KEYOUT2_PIN, GPIO_PIN_SET);
    	break;
    }
}
// Edit with pot parameters
void editSetup() {
  static int editstage=0;
  if(++editstage > 10) {
	  editstage = 0;
  }
  switch(editstage) {
  case 0:
	  envPhaseRate = 0x100000 / (adcVal[0] + 0x100);
	  break;
  case 1:
	  printf("%d\r\n", adcVal[1]);
	  oscMix2 = ((adcVal[1] & 1023) >> 2);
	  oscMix1 = 256 - oscMix2;
	  if(adcVal[1] >= 3072) {
		  modType = 1;
		  waveTab1 = waveTabD;
		  waveTab2 = waveTabD;
	  }
	  else if(adcVal[1] >= 2048) {
		  modType = 0;
		  waveTab1 = waveTabC;
		  waveTab2 = waveTabD;
	  }
	  else if(adcVal[1] >= 1024) {
		  modType = 0;
		  waveTab1 = waveTabB;
		  waveTab2 = waveTabC;
	  }
	  else {
		  modType = 0;
		  waveTab1 = waveTabA;
		  waveTab2 = waveTabB;
	  }
	  break;
  case 2:
	  int detune = (adcVal[2] & 0xfff) - 2048;
	  oscFsTune = OSCFSORIG - detune;
	  break;
  }
}
// Create the timings for keyscan / editparameters
void gpioInterval() {
	// 1kHz Interval
	static int cntKeyScan = 0;
	static int cntEditParam = 0;
	if(++cntKeyScan > 1) {
		cntKeyScan = 0;
		scanKeys();
	}
	else if(++cntEditParam >= 8) {
    	cntEditParam = 0;
    	editSetup();
    }
}
// Generate output signals
//  fill half the buffer with the signals from offset
void generate(int offset) {
	int wavOut;
//	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
	ledOut[0] = ledOut[1] = ledOut[2] = 0;
	for(int j = 0; j < OUTBUFFLENHALF; ++j) {
		wavOut = 0x8000;
		for(int i = 0; i < VOICEMAX; ++i) {
			if(envPhaseActive[i] < ENVPHASEMAX) {
				int ph = oscPhaseActive[i];
				int frac = (ph >> 6) & 0xf;
				int idx = ph >> 10;
				int idxNext = idx + 1;
				int w1 = waveTab1[idx] + (((waveTab1[idxNext] - waveTab1[idx]) * frac) >> 4);
				int w2;
				switch(modType) {
				case 2:
					modPhase = (modPhase + modDelta) & 0xffff;
					int s = waveTab1[modPhase>>10];
					w2 = (w1 * (s>>2)) >> 8;
					w1 = (w1 * (modPhase>>8)) >> 8;
					break;
				case 1:
					modPhase = (modPhase + modDelta) & 0xffff;
					int ss = waveTab1[modPhase>>10];
//					w2 = (w1 * (ss>>2)) >> 8;
					w2 = (w1 * (modPhase>>8)) >> 8;
					break;
				case 0:
					w2 = waveTab2[idx] + (((waveTab2[idxNext] - waveTab2[idx]) * frac) >> 4);
					break;
				}
				int w = w1 + (((w2 - w1) * oscMix2) >> 8);
				int envIdx = (envPhaseActive[i] >> 20);
				int envIdxNext = envIdx + 1;
				int envFrac = (envPhaseActive[i] >> 16) & 0xf;
				int envVal = envTab[envIdx];
				envVal = envVal + ((envTab[envIdxNext] - envVal) * envFrac >> 4);
				int dat = ((w * (envVal>>2)) >> 5);
				int ledidx = key2LedOut[keyActive[i]];
				if(envVal > ledOut[ledidx])
					ledOut[ledidx] = envVal;
				wavOut += dat;
				if((oscPhaseActive[i] += oscDelta[keyActive[i]]) >= 0x10000) {
					oscPhaseActive[i] -= 0x10000;
				}
				envPhaseActive[i] += envPhaseRate;
				if(keyActiveQue[i] >= 0) {
					if((envPhaseActive[i] += 0x100000) >= ENVPHASEMAX) {
						keyActive[i] = keyActiveQue[i];
						envPhaseActive[i] = 0;
						oscPhaseActive[i] = 0;
						keyActiveQue[i] = -1;
					}
				}
			}
		}
		if(wavOut >= 0xf000) {
			wavOut = 0xf000;
//			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
		}
		else if(wavOut < 0x1000) {
			wavOut = 0x1000;
//			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
		}
		else {
//			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
		}
		outBuff[offset + j] = wavOut;
		if(offset) {
			outBuffAvail1 = 1;
//			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
		}
		else {
			outBuffAvail0 = 1;
//			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
		}
	}
//	printf("%d\r\n",wavOutMax);
	for(int key = 0; key < 17; ++key)
		oscDelta[key] = oscFreq[key] / oscFsTune;
	neoPixelSetCol(3, ((ledOut[1]<<10)&0x0f0000) | ((ledOut[2]<<2)&0x000f00) | (ledOut[0] >> 3));
//	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);

}

//###### Callback #######################

//###### TIM17 callback
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if(htim->Instance == TIM17) {
		if(tim17prescaler == 1) {
			__HAL_TIM_SET_PRESCALER(htim, tim17prescaler = 1000);
			HAL_TIM_PWM_Start_DMA(&htim17,TIM_CHANNEL_1,(uint32_t*)neoPixelBuffNone, sizeof neoPixelBuffNone);
		}
		else {
			__HAL_TIM_SET_PRESCALER(htim, tim17prescaler = 1);
			HAL_TIM_PWM_Start_DMA(&htim17,TIM_CHANNEL_1,(uint32_t*)neoPixelBuff, sizeof neoPixelBuff);
		}
	}
}
//###### TIM7 callback
void  HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	if(htim->Instance == TIM7) {
	  gpioInterval();
	}
}
//###### DAC callback End of buffer
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac) {
	outBuffAvail1 = 0;
}
//###### DAC callback End of half buffer
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac) {
	outBuffAvail0 = 0;

}
//###### for printf debug ######
int _write(int file, char *ptr, int len)
{
  HAL_UART_Transmit(&huart2,(uint8_t *)ptr,len,10);
  return len;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  setbuf(stdout, NULL);

  for(int i = 0; i < OUTBUFFLEN; ++i) {
	  outBuff[i] = 0x8000;
  }
  for(int i = 0; i < VOICEMAX; ++i) {
	  envPhaseActive[i] = (ENVTABLEN << 20);
  }
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM17_Init();
  MX_ADC1_Init();
  MX_DAC1_Init();
  MX_TIM6_Init();
  MX_TIM7_Init();
  /* USER CODE BEGIN 2 */

  printf(" USER CODE BEGIN 2\r\n");

  HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adcVal, 3);
  HAL_TIM_Base_Start_IT(&htim6);
  HAL_TIM_Base_Start_IT(&htim7);
  HAL_TIM_PWM_Start_DMA(&htim17,TIM_CHANNEL_1,(uint32_t*)neoPixelBuff, sizeof neoPixelBuff);

  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);

  for(int i = 0; i < 17; ++i) {
	  oscDelta[i] = oscFreq[i] / OSCFSORIG;
  }
  for(int i = 0; i < VOICEMAX; ++i) {
	  keyActive[i] = keyActiveQue[i] = -1;
  }
  neoPixelSetCol(3, 0x101010);
  HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)outBuff, OUTBUFFLEN, DAC_ALIGN_12B_L);

  printf("STM32 Initialized.\r\n");


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  printf("BEGIN WHILE\r\n");

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  // Fill output buffer
	  if(outBuffAvail0 == 0)
		  generate(0);
	  if(outBuffAvail1 == 0)
		  generate(OUTBUFFLENHALF);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 3;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_601CYCLES_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief DAC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC1_Init(void)
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */

  /** DAC Initialization
  */
  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  /* USER CODE END DAC1_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 2666;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief TIM7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM7_Init(void)
{

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 64;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 1000;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */

}

/**
  * @brief TIM17 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM17_Init(void)
{

  /* USER CODE BEGIN TIM17_Init 0 */

  /* USER CODE END TIM17_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM17_Init 1 */

  /* USER CODE END TIM17_Init 1 */
  htim17.Instance = TIM17;
  htim17.Init.Prescaler = 1;
  htim17.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim17.Init.Period = 39;
  htim17.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim17.Init.RepetitionCounter = 0;
  htim17.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim17) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim17) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim17, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim17, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM17_Init 2 */

  /* USER CODE END TIM17_Init 2 */
  HAL_TIM_MspPostInit(&htim17);

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8|GPIO_PIN_11|GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);

  /*Configure GPIO pin : PF1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB4 PB5
                           PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA11 PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
