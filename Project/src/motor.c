#include "motor.h"
#include "encoder.h"
#include "nfv2.h"

extern MOTOR_St				Motor;
extern NF_STRUCT_ComBuf 	NFComBuf;

void MOTOR_Proc(void) {
	static int32_t prevPosition = 0;

	Motor.position = ENCODER1_Position();
	Motor.speed = Motor.position - prevPosition;
	prevPosition = Motor.position;

	NFComBuf.ReadDrivesPosition.data[0] = Motor.position;
}

// BLDC motor steps
// every row from 1 to 6 is called by a hall state
// every column a FET from 3-phase bridge
// motor off is at row 0 BLDC_BRIDGE_STATE_VORWARD[0]
// cw - rechtslauf - positiv
// {    1H,1L      ,      2H,2L      ,     3H,3L    }
// BLDC motor steps
// every row from 1 to 6 is one of the 6 motor vector state
// every column a FET from 3-phase bridge
// all FETs off at row 0 or 8 (this pattern should not come from the hallsensor)
// cw - rechtslauf - positiv
// {    1H,1L      ,      2H,2L      ,     3H,3L    }
static const u8 BLDC_BRIDGE_STATE_FORWARD[8][6] =   // Motor step
{
//									BRIDGE	SEQ		HALL
//												mik.net	Parvalux
   { 0,0   ,   0,0   ,  0,0 },  //	. . .	0	000		000
   { 0,0   ,   0,1   ,  1,0 },  //	. L H	4	010		001
   { 0,1   ,   1,0   ,  0,0 },  //	L H .	2	001		010
   { 0,1   ,   0,0   ,  1,0 },  //	L . H	3	011		011
   { 1,0   ,   0,0   ,  0,1 },  //	H . L	6	100		100
   { 1,0   ,   0,1   ,  0,0 },  //	H L .	5	110		101
   { 0,0   ,   1,0   ,  0,1 },  //	. H L	1	101		110
   { 0,0   ,   0,0   ,  0,0 }  //	. . .	0	111		000
};
static const u8 BLDC_BRIDGE_STATE_REVERSE[8][6] =   // Motor step
{
//									BRIDGE	SEQ		HALL
//												mik.net	Parvalux
   { 0,0   ,   0,0   ,  0,0 },  //	. . .	0	000		000
   { 0,0   ,   1,0   ,  0,1 },  //	. H L	4	010		001
   { 1,0   ,   0,1   ,  0,0 },  //	H L .	2	001		010
   { 1,0   ,   0,0   ,  0,1 },  //	H . L	3	011		011
   { 0,1   ,   0,0   ,  1,0 },  //	L . H	6	100		100
   { 0,1   ,   1,0   ,  0,0 },  //	L H .	5	110		101
   { 0,0   ,   0,1   ,  1,0 },  //	. L H	1	101		110
   { 0,0   ,   0,0   ,  0,0 }  //	. . .	0	111		000
};
static const u8 BLDC_BRIDGE_STATE_STILL[8][6] =   // Motor step
{
//									BRIDGE	SEQ		HALL
//												mik.net	Parvalux
   { 0,0   ,   0,0   ,  0,0 },  //	. . .	0	000		000
   { 0,0   ,   0,0   ,  0,0 },  //	. . .	4	010		001
   { 0,0   ,   0,0   ,  0,0 },  //	. . .	2	001		010
   { 0,0   ,   0,0   ,  0,0 },  //	. . .	3	011		011
   { 0,0   ,   0,0   ,  0,0 },  //	. . .	6	100		100
   { 0,0   ,   0,0   ,  0,0 },  //	. . .	5	110		101
   { 0,0   ,   0,0   ,  0,0 },  //	. . .	1	101		110
   { 0,0   ,   0,0   ,  0,0 }  //	. . .	0	111		000
};

void MOTOR_Config(void) {
	//Configuration Structures
//	NVIC_InitTypeDef		NVIC_InitStructure;
	GPIO_InitTypeDef		GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef	TIM_TimeBaseStructure;
	TIM_OCInitTypeDef		TIM_OCInitStructure;
//	TIM_ICInitTypeDef		TIM_ICInitStructure;
	TIM_BDTRInitTypeDef		TIM_BDTRInitStructure;

	// TIM1 clock enable
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	// AFIO Clock Enable
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	// GPIO clocks enable
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);

	// TIM1 Full Pin Remap
	GPIO_PinRemapConfig(GPIO_FullRemap_TIM1, ENABLE);

	/*	PORT E Push-Pull Outputs:				*\
		PE.9	(TIM1_CH1)	HIN1
		PE.11	(TIM1_CH2)	HIN2
		PE.13	(TIM1_CH3)	HIN3
		PE.8	(TIM1_CH1N)	LIN1
		PE.10	(TIM1_CH2N)	LIN2
		PE.12	(TIM1_CH3N)	LIN3				*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOE, &GPIO_InitStructure);
 
	// Time Base configuration
	TIM_TimeBaseStructure.TIM_Prescaler = 3;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = 1200;		// 24MHz / 1200 = 20kHz
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);
 
	// Channel 1, 2, 3 set to PWM mode - all 6 outputs
	// per channel on output is  low side fet, the opposite is for high side fet

	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 500; // BLDC_ccr_val

	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
	TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Set;

	TIM_OC1Init(TIM1, &TIM_OCInitStructure);
	TIM_OC2Init(TIM1, &TIM_OCInitStructure);
	TIM_OC3Init(TIM1, &TIM_OCInitStructure);

	// activate preloading the CCR register
	TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
	TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);
	TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);
 
	/* automatic output enable, break off, dead time ca. 200ns and  
	// no lock of configuration */
	TIM_BDTRInitStructure.TIM_OSSRState = TIM_OSSRState_Enable;
	TIM_BDTRInitStructure.TIM_OSSIState = TIM_OSSIState_Enable;
	TIM_BDTRInitStructure.TIM_LOCKLevel = TIM_LOCKLevel_OFF;

	// DeadTime value n=1 bis 31: from 14ns to 1,7us
	// DeadTime value n=129 bis 159: from 1,7us to 3,5ms
	// DeadTime value 7 => 98ns
	// ... see programmers reference manual
	// DeadTime[ns] = value * (1/SystemCoreFreq) (on 72MHz: 7 is 98ns, on 24MHz: 7 is 296ns)
	TIM_BDTRInitStructure.TIM_DeadTime = 7;
	TIM_BDTRInitStructure.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable;
	 
	// enabel this if you use emergency stop signal
	// TIM_BDTRInitStructure.TIM_Break = TIM_Break_Enable;
	// TIM_BDTRInitStructure.TIM_BreakPolarity = MOTOR_TMC603_EMSTOP_POLARITY; 
	TIM_BDTRInitStructure.TIM_Break = TIM_Break_Disable;
	TIM_BDTRConfig(TIM1, &TIM_BDTRInitStructure);
 
	// preload ARR register
	TIM_CCPreloadControl(TIM1, ENABLE);

	// activate COM (Commutation) Event from Slave (HallSensor timer)
	// through TRGI
	TIM_SelectCOM(TIM1, ENABLE);

  // Internal connection from Hall/Enc Timer to Motor Timer
  // eg. TIM1 (BLDC Motor Timer) is Slave of TIM3 (Hall Timer)
  // Internal connection from Hall/Enc Timer to Motor Timer

  // Choose carefully from the following possible combination
  // check programmers reference manual
  // TIM_SelectInputTrigger(TIM1, TIM_TS_ITR0);
  // MotorTimer = TIM1, HallTimer = TIM5
  // TIM_SelectInputTrigger(TIM1, TIM_TS_ITR1);
  // MotorTimer = TIM1, HallTimer = TIM2

	//TIM_SelectInputTrigger(TIM1, TIM_TS_ITR2);
	// MotorTimer = TIM1, HallTimer = TIM3

	TIM_SelectInputTrigger(TIM1, TIM_TS_ITR3);
	// MotorTimer = TIM1, HallTimer = TIM4

  // TIM_SelectInputTrigger(TIM8, TIM_TS_ITR0);
  // MotorTimer = TIM8, HallTimer = TIM1
  // TIM_SelectInputTrigger(TIM8, TIM_TS_ITR1);
  // MotorTimer = TIM8, HallTimer = TIM2
  // TIM_SelectInputTrigger(TIM8, TIM_TS_ITR2);
  // MotorTimer = TIM8, HallTimer = TIM4
  // TIM_SelectInputTrigger(TIM8, TIM_TS_ITR3);
  // MotorTimer = TIM8, HallTimer = TIM5
 
//	// Enable interrupt, motor commutation has high piority and has
//	// a higher subpriority then the hall sensor
//	NVIC_InitStructure.NVIC_IRQChannel = TIM1_TRG_COM_IRQn;
//	// highest priority
//	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;
//	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
//	// highest priority
//	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
//	NVIC_Init(&NVIC_InitStructure);
 
  // Interrupt for hardwired EmergencyStop (if needed)
  // Timer 1 Motor Emergency Break Input
  // NVIC_InitStructure.NVIC_IRQChannel = TIM1_BRK_IRQn;
  // NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;
  // NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  // NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  // NVIC_Init(&NVIC_InitStructure);
 
  // --------- activate the bldc bridge ctrl. ----------
  // in a project this will be done late after complete
  // configuration of other peripherie
 
//	// enable COM (commutation) IRQ
//	TIM_ITConfig(TIM1, TIM_IT_COM, ENABLE);
 
	// enable motor timer
	TIM_Cmd(TIM1, ENABLE);

	// enable motor timer main output (the bridge signals)
	TIM_CtrlPWMOutputs(TIM1, ENABLE);

	// Data structures init
	Motor.setPWM = 0;
	Motor.setSpeed = 0;
	Motor.setPosition = 0;
	Motor.setCurrent = 0;
}
 
// enable the connection between HallTimer and MotorTimer
void enableHallCommutateSignal() {
   TIM_SelectCOM(TIM1, ENABLE);
}
 
// disable the connection between HallTimer and MotorTimer
void disableHallCommutateSignal() {
   TIM_SelectCOM(TIM1, DISABLE);
}
 
// This function handles motor timer trigger and commutation interrupts
// can be used for calculation...
void TIM1_TRG_COM_IRQHandler(void)
{
  TIM_ClearITPendingBit(TIM1, TIM_IT_COM);
  // commutationCount++;
}
 
/* This is called from HALL timer interrupt handler
   remember:
     if hall a hall edge is detected
     first the motor commutation event is done
     next this routine is called which has to prepare the next motor step
     (which FET must be switched on or off)
   active freewhelling is used to minimize power loss */

void BLDCMotorPrepareCommutation(void)
{
	vu8			hallpos;
	vu8			BH1, BL1, BH2, BL2, BH3, BL3;
	const u8 	(*bldcBridgeState)[6];
	vs16 		pwm;

	hallpos = HALL_Pattern();

	if(Motor.setPWM == 0){
		bldcBridgeState = BLDC_BRIDGE_STATE_STILL;
		pwm = 0;
	}
	else if(Motor.setPWM > 0){
		bldcBridgeState = BLDC_BRIDGE_STATE_FORWARD;
		pwm = Motor.setPWM;
	}
	else{
		bldcBridgeState = BLDC_BRIDGE_STATE_REVERSE;
		pwm = - Motor.setPWM;
	}

	// this is only for motor direction forward

	BH1 = bldcBridgeState[hallpos][0];
	BL1 = bldcBridgeState[hallpos][1];

	BH2 = bldcBridgeState[hallpos][2];
	BL2 = bldcBridgeState[hallpos][3];

	BH3 = bldcBridgeState[hallpos][4];
	BL3 = bldcBridgeState[hallpos][5];


	// Bridge FETs for Motor Phase U
	if (BH1) {
		TIM1->CCR1 = pwm;
		TIM_CCxNCmd(TIM1, TIM_Channel_1, TIM_CCxN_Enable);
	} else {
		TIM1->CCR1 = 0;
		if (BL1){
			TIM_CCxNCmd(TIM1, TIM_Channel_1, TIM_CCxN_Enable);
		} else {
			TIM_CCxNCmd(TIM1, TIM_Channel_1, TIM_CCxN_Disable);
		}
	}

	// Bridge FETs for Motor Phase V
	if (BH2) {
		TIM1->CCR2 = pwm;
		TIM_CCxNCmd(TIM1, TIM_Channel_2, TIM_CCxN_Enable);
	} else {
		TIM1->CCR2 = 0;
		if (BL2){
			TIM_CCxNCmd(TIM1, TIM_Channel_2, TIM_CCxN_Enable);
		} else {
			TIM_CCxNCmd(TIM1, TIM_Channel_2, TIM_CCxN_Disable);
		}
	}

	// Bridge FETs for Motor Phase W
	if (BH3) {
		TIM1->CCR3 = pwm;
		TIM_CCxNCmd(TIM1, TIM_Channel_3, TIM_CCxN_Enable);
	} else {
		TIM1->CCR3 = 0;
		if (BL3){
			TIM_CCxNCmd(TIM1, TIM_Channel_3, TIM_CCxN_Enable);
		} else {
			TIM_CCxNCmd(TIM1, TIM_Channel_3, TIM_CCxN_Disable);
		}
	}
}

void MOTOR_SetPWM(s16 pwm) {
	if(pwm > MAX_PWM)
		pwm = MAX_PWM;
	else if(pwm < -MAX_PWM)
		pwm = -MAX_PWM;
	Motor.setPWM = pwm;
	BLDCMotorPrepareCommutation();
	TIM_GenerateEvent(TIM1, TIM_EventSource_COM);
}
