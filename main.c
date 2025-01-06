#include "stm8s.h"

#define MOTOR_SPEED_COUNT 4

#define FRONT_PANEL_BUTTON_COUNT 5
typedef enum
{
	BTN_LIGHT_OFF = 0, BTN_LIGHT_ON = 2, BTN_SPEED_DOWN = 3, BTN_SPEED_UP = 4, BTN_SENSE = 5
} FrontPanelButton_TypeDef;
const FrontPanelButton_TypeDef frontPanelButtons[FRONT_PANEL_BUTTON_COUNT] = 
{
	BTN_LIGHT_OFF, BTN_LIGHT_ON, BTN_SPEED_DOWN, BTN_SPEED_UP, BTN_SENSE
};

#define FRON_PANEL_LED_COUNT 6
typedef enum
{
	LED_SPEED_1 = 0, LED_SPEED_2 = 1, LED_SPEED_3 = 2, LED_SPEED_4 = 3, LED_SENSE_RED = 4, LED_SENSE_GREEN = 5
} FrontPanelLed_TypeDef;
const FrontPanelButton_TypeDef frontPanelLeds[FRON_PANEL_LED_COUNT] = 
{
	LED_SPEED_1, LED_SPEED_2, LED_SPEED_3, LED_SPEED_4, LED_SENSE_RED, LED_SENSE_GREEN
};

typedef struct {
    GPIO_TypeDef* port;
    GPIO_Pin_TypeDef pin;
		GPIO_Mode_TypeDef mode;
} PortPin_TypeDef;

#define LED_BUTTON_OUT_PORT_PIN_COUNT 6
const PortPin_TypeDef ledButtonOutPortPins[LED_BUTTON_OUT_PORT_PIN_COUNT] =
{
	{GPIOC, GPIO_PIN_3, GPIO_MODE_OUT_OD_HIZ_FAST},  // LED_SPEED_1 / BTN_LIGHT_OFF
	{GPIOA, GPIO_PIN_1, GPIO_MODE_OUT_OD_HIZ_FAST},  // LED_SPEED_2
	{GPIOD, GPIO_PIN_5, GPIO_MODE_OUT_OD_HIZ_FAST},  // LED_SPEED_3 / BTN_LIGHT_ON
	{GPIOA, GPIO_PIN_2, GPIO_MODE_OUT_OD_HIZ_FAST},  // LED_SPEED_4 / BTN_SPEED_DOWN
	{GPIOD, GPIO_PIN_6, GPIO_MODE_OUT_OD_HIZ_FAST},  // LED_SENSE_RED / BTN_SPEED_UP
	{GPIOA, GPIO_PIN_3, GPIO_MODE_OUT_OD_HIZ_FAST}   // LED_SENSE_GREEN / BTN_SENSE
};

#define RELAY_SWITCH_COUNT 4
typedef enum
{
	SWITCH_MOTOR_ON = 0, SWITCH_MOTOR_SPEED_0 = 1, SWITCH_MOTOR_SPEED_1 = 2, SWITCH_LIGHT_ON = 3
} RelaySwitch_TypeDef;

const PortPin_TypeDef relaySwitchPortPins[RELAY_SWITCH_COUNT] =
{
	{GPIOB, GPIO_PIN_4, GPIO_MODE_OUT_OD_LOW_SLOW},	// SWITCH_MOTOR_ON
	{GPIOC, GPIO_PIN_5, GPIO_MODE_OUT_PP_LOW_SLOW}, // SWITCH_MOTOR_SPEED_0
	{GPIOC, GPIO_PIN_6, GPIO_MODE_OUT_PP_LOW_SLOW},	// SWITCH_MOTOR_SPEED_1
	{GPIOC, GPIO_PIN_7, GPIO_MODE_OUT_PP_LOW_SLOW}  // SWITCH_LIGHT_ON
};


void AWU_Config(void)
{
		AWU_LSICalibrationConfig(128000);
    AWU_Init(AWU_TIMEBASE_32MS);
}

void CLK_Config(void)
{
	CLK_SYSCLKConfig(CLK_PRESCALER_HSIDIV1);
	CLK_ClockSwitchConfig(CLK_SWITCHMODE_AUTO, CLK_SOURCE_LSI, DISABLE, CLK_CURRENTCLOCKSTATE_ENABLE);
}

void GPIO_Config(void)
{
	int8_t i;
		
	for (i = 0; i < LED_BUTTON_OUT_PORT_PIN_COUNT; i++) {
		PortPin_TypeDef port_pin = ledButtonOutPortPins[i];
		GPIO_Init(port_pin.port, port_pin.pin, port_pin.mode);
	}
	// PC4 - TEST_CURRENT_BNT
	GPIO_Init(GPIOC, GPIO_PIN_4, GPIO_MODE_IN_PU_NO_IT);
	
	// Relay switch ports
	for (i = 0; i < RELAY_SWITCH_COUNT; i++) {
		PortPin_TypeDef port_pin = relaySwitchPortPins[i];
		GPIO_Init(port_pin.port, port_pin.pin, port_pin.mode);
	}
}

void switch_relay(RelaySwitch_TypeDef relaySwitch, bool enabled) 
{
	PortPin_TypeDef port_pin = relaySwitchPortPins[relaySwitch];
	
	if (enabled)
	{
		GPIO_WriteHigh(port_pin.port, port_pin.pin);
	}
  else
	{
		GPIO_WriteLow(port_pin.port, port_pin.pin);
	}
}

int8_t read_button_pressed(void)
{
	int8_t pressed_button_index = -1;
	int8_t i;
	
	int8_t pa_data = GPIOA->ODR;
	int8_t pb_data = GPIOB->ODR;
	int8_t pc_data = GPIOC->ODR;
	int8_t pd_data = GPIOD->ODR;
	
	// increase mcu clock to avoid led twinkling
	CLK_ClockSwitchConfig(CLK_SWITCHMODE_AUTO, CLK_SOURCE_HSI, DISABLE, CLK_CURRENTCLOCKSTATE_ENABLE);
	// disable all led/button ports
	for (i = 0; i < LED_BUTTON_OUT_PORT_PIN_COUNT; i++) {
		PortPin_TypeDef port_pin = ledButtonOutPortPins[i];
		port_pin.port->ODR |= port_pin.pin;
	}
	
	// select and test buttons one by one
	for (i = 0; i < FRONT_PANEL_BUTTON_COUNT; i++) {
		PortPin_TypeDef port_pin = ledButtonOutPortPins[frontPanelButtons[i]];
		GPIO_TypeDef* port = port_pin.port;
		GPIO_Pin_TypeDef pin = port_pin.pin;
		uint8_t in_port_data;

		port->ODR &= ~pin;
		in_port_data = GPIOC->IDR;
		port->ODR |= pin;

		if (!(in_port_data & GPIO_PIN_4))
		{
			if (pressed_button_index >= 0)
			{
				pressed_button_index = -2;
				break;
			}
			pressed_button_index = i;
		}
	}
	GPIOA->ODR = pa_data;
	GPIOB->ODR = pb_data;
	GPIOC->ODR = pc_data;
	GPIOD->ODR = pd_data;
	
	CLK_ClockSwitchConfig(CLK_SWITCHMODE_AUTO, CLK_SOURCE_LSI, DISABLE, CLK_CURRENTCLOCKSTATE_ENABLE);

	return pressed_button_index;
}

void main(void)
{
	bool light_enabled = FALSE;
	int8_t motor_speed = 0;
	bool front_panel_ready_for_input = FALSE;
	int8_t i;

	GPIO_Config();
	CLK_Config();
	AWU_Config();

	while(TRUE)
	{
		int8_t buton_pressed_index = read_button_pressed();
		if (front_panel_ready_for_input && buton_pressed_index >= 0)
		{
			// process buttom pressed
			switch(frontPanelButtons[buton_pressed_index])
			{
				case BTN_LIGHT_OFF: light_enabled = FALSE; break;
				case BTN_LIGHT_ON: light_enabled = TRUE; break;
				case BTN_SPEED_DOWN: motor_speed = motor_speed > 0 ? motor_speed - 1 : 0; break;
				case BTN_SPEED_UP: motor_speed = motor_speed < MOTOR_SPEED_COUNT ? motor_speed + 1 :  MOTOR_SPEED_COUNT; break;
				case BTN_SENSE: break;
			};
		}
			
		front_panel_ready_for_input = buton_pressed_index == -1;
		
		// set outputs according to state
		for (i = 0; i < motor_speed; i++) {
			PortPin_TypeDef port_pin = ledButtonOutPortPins[frontPanelLeds[i + LED_SPEED_1]];
			GPIO_WriteLow(port_pin.port, port_pin.pin);
		}
		
		for (i = motor_speed; i < MOTOR_SPEED_COUNT; i++) {
			PortPin_TypeDef port_pin = ledButtonOutPortPins[frontPanelLeds[i + LED_SPEED_1]];
			GPIO_WriteHigh(port_pin.port, port_pin.pin);
		}
		
		switch_relay(SWITCH_MOTOR_ON, motor_speed > 0);

		switch_relay(SWITCH_MOTOR_SPEED_0, (motor_speed > 0) && ((motor_speed - 1) & 0x02));
		switch_relay(SWITCH_MOTOR_SPEED_1, (motor_speed > 0) && !((motor_speed - 1) & 0x01));

		switch_relay(SWITCH_LIGHT_ON, light_enabled);

		wfi();
	}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{ 
  while (1);
}
#endif



