#include "bsp_buzzer.h"
#include "main.h"
#include "stm32f4xx_hal.h"
extern TIM_HandleTypeDef htim4;
void buzzer_on(uint16_t psc, uint16_t pwm)
{
	__HAL_TIM_PRESCALER(&htim4, psc);
	//__HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_3, pwm);
  __HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_3, 0);
}
void buzzer_off(void)
{
	__HAL_TIM_SetCompare(&htim4, TIM_CHANNEL_3, 0);
}


int musicB[]={1800,1200,600,0};
int musicC[]={0,600,1200,1800};
int musicD[]={0,300,450,900};
int musicE[]={0,200,300,400};


int yu;


void ConfigureTimerPrescaler(TIM_HandleTypeDef *htim, uint32_t prescaler, uint32_t reloadMode) {
	// 设置定时器的预分频器值
	htim->Init.Prescaler = prescaler;
	
	// 设置定时器的预分频器重载模式
	htim->Init.Period = 199;
	
	// 初始化定时器
	HAL_TIM_Base_Init(htim);
}


void Sound_SetHZ(uint16_t a)
{
ConfigureTimerPrescaler(&htim4, a,199);
}


void Play_Music(int a,int b,int c)
{
Sound_SetHZ(a);HAL_Delay(b);Sound_SetHZ(20);HAL_Delay(c);
}


void B_Music(void)
{
for(int i=0;i<=6;i++)
{
Play_Music(musicB[i],50,0);
}
}
void C_Music(void)
{
	for(int i=0;i<=6;i++)
	{
		Play_Music(musicC[i],100,0);
	}
}

void D_Music(void)
{
	for(int i=0;i<=6;i++)
	{
		Play_Music(musicD[i],100,0);
	}
}

void E_Music(void)
{
	for(int i=0;i<=6;i++)
	{
		Play_Music(musicE[i],100,0);
	}
}



