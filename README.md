# MS51_PWM_breathing_LED
MS51_PWM_breathing_LED

update @ 2025/10/07

1. MODIFY PWM5 , PWM5L page switch

2. add define USE_FLASH_TBL , USE_RAM_TBL

- USE_FLASH_TBL : duty table place in flash size , to reduce ram size

- USE_RAM_TBL : duty table place in ram buffer


update @ 2025/10/04

1.init 2 PWM in MS51FB9AE EVB , with freq : 100Hz , to execute breathing LED

- enable USE_P12_PWM0_CH0 , for P1.2/PWM0_CH0 ( MS51 EVB LED )

- enable USE_P15_PWM0_CH5 , for P1.5/PWM0_CH5

3. project split as below , choose the target project and compile

- PERIOD_1000MS

- PERIOD_2000MS
        
- PERIOD_3750MS (DEFAULT)

- PERIOD_5000MS
        
- PERIOD_10000MS

- PERIOD_14000MS	