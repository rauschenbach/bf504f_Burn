PROGNAME = GNS110_bf504f
CC_DIR = C:/Program Files (x86)/Analog Devices/VisualDSP 5.1.2
OBJCOPY_DIR = ..\utils
CC = ${CC_DIR}/ccblkfn.exe
AS = ${CC_DIR}/easmBLKFN.exe
OBJCOPY =  ${OBJCOPY_DIR}/objcopy

ifeq ($(quartz_freq),19MHz)
QUARTZ_FREQ=19200000
CORE_CLOCK = 60000000
RELEASE_FOLDER = Release_19MHz
endif


ifeq ($(quartz_freq),8MHz)
QUARTZ_FREQ=8192000
CORE_CLOCK = 49152000
RELEASE_FOLDER = Release_8MHz
endif


REVISION = -si-revision 0.1

INC_DIRS = adc;config;drivers;irq;log;modem;periph;pll;sdcard;utils

CFLAGS = -Os -DQUARTZ_CLK_FREQ=${QUARTZ_FREQ} -structs-do-not-overlap -no-multiline -D__PROCESSOR_SPEED__=${CORE_CLOCK} -double-size-32 -I${INC_DIRS} \
                     -decls-strong -warn-protos ${REVISION} -proc ADSP-BF504F -file-attr ProjectName=${PROGNAME}
ASFLAG = -proc ADSP-BF504F ${REVISION} -MM -file-attr ProjectName={PROGNAME} 
LDFLAG = -flags-link -MDUSER_CRT=ADI_QUOTEGNS110_bf504f_basiccrt.dojADI_QUOTE,-MD__cplusplus -flags-link -e \
         -flags-link -ev -flags-link -od,.\${RELEASE_FOLDER} -proc ADSP-BF504F  ${REVISION} -flags-link -MM


OBJS = $(addprefix ${RELEASE_FOLDER}/,ads1282.doj am0.doj am3.doj amt.doj bmp085.doj bq32k.doj diskio.doj circbuf.doj comport.doj crc16.doj \
		dac.doj eeprom.doj ff.doj ffunicode.doj flash.doj GNS110_bf504f_basiccrt.doj GNS110_bf504f_heaptab.doj gps.doj \
		irq.doj led.doj log.doj lsm303.doj main.doj modem.doj pll.doj ports.doj power.doj rele.doj rsi.doj sintab.doj \
		spi0.doj spi1.doj sport0.doj tests.doj timer0.doj timer1.doj timer2.doj timer3.doj \
		timer4.doj twi.doj uart0.doj uart1.doj utils.doj xpander.doj)

TMPFILES =  *.c~ *.h~

all: ${RELEASE_FOLDER}/loader.ram ${RELEASE_FOLDER}/loader.rom
###		mv  .${RELEASE_FOLDER}/loader.ram burn_19MHz/loader.ram
###		mv  .${RELEASE_FOLDER}/loader.rom burn_19MHz/loader.rom

${RELEASE_FOLDER}/loader.ram: ${RELEASE_FOLDER}/${PROGNAME}.dxe Makefile
	${CC_DIR}/elfloader.exe ${RELEASE_FOLDER}\${PROGNAME}.dxe -b Flash -f binary -Width 16 \
                   -o ${RELEASE_FOLDER}/loader.ram ${REVISION} -proc ADSP-BF504F -MM

${RELEASE_FOLDER}/loader.rom: ${RELEASE_FOLDER}/${PROGNAME}.dxe Makefile
	${CC_DIR}/elfloader.exe ${RELEASE_FOLDER}\${PROGNAME}.dxe -romsplitter -maskaddr 21 -b Flash -f HEX -Width 16 -o ${RELEASE_FOLDER}/loader2.hex ${REVISION} -proc ADSP-BF504F -MM
	${OBJCOPY} -I ihex -O binary ${RELEASE_FOLDER}/loader2.hex ${RELEASE_FOLDER}/loader.rom

${RELEASE_FOLDER}/${PROGNAME}.dxe: ${OBJS}
	${CC} $^ -o $@ -T system/${PROGNAME}.ldf -map ${RELEASE_FOLDER}\${PROGNAME}.map.xml -flags-link -xref -L .\${RELEASE_FOLDER} ${LDFLAG}

clean:
	@del /F /Q $(subst /,\,${OBJS}) ${RELEASE_FOLDER}\${PROGNAME}* ${RELEASE_FOLDER}\*.xml ${RELEASE_FOLDER}\*ldr ${TMPFILES} ${RELEASE_FOLDER}\*.r*m ${RELEASE_FOLDER}\*.hex
	


${RELEASE_FOLDER}/ads1282.doj: adc/ads1282.c adc/ads1282.h globdefs.h config.h version.h pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@


${RELEASE_FOLDER}/am0.doj: modem/am0.c modem/am0.h globdefs.h config.h version.h pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/am3.doj: modem/am3.c modem/am3.h globdefs.h config.h version.h pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@


${RELEASE_FOLDER}/amt.doj: modem/amt.c modem/amt.h globdefs.h config.h version.h pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/bmp085.doj: drivers/bmp085.c drivers/bmp085.h globdefs.h config.h version.h pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/bq32k.doj: drivers/bq32k.c drivers/bq32k.h globdefs.h config.h version.h pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/xpander.doj: drivers/xpander.c drivers/xpander.h globdefs.h config.h version.h   pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/diskio.doj: sdcard/diskio.c sdcard/diskio.h globdefs.h sdcard/ffconf.h version.h pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/ff.doj: sdcard/ff.c sdcard/ff.h globdefs.h sdcard/ffconf.h version.h pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/ffunicode.doj: sdcard/ffunicode.c sdcard/ff.h sdcard/ffconf.h version.h pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@


${RELEASE_FOLDER}/circbuf.doj: utils/circbuf.c utils/circbuf.h globdefs.h config.h version.h pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/utils.doj: utils/utils.c utils/utils.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/comport.doj: drivers/comport.c drivers/comport.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/crc16.doj: utils/crc16.c utils/crc16.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/tests.doj: utils/tests.c utils/tests.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/sintab.doj: utils/sintab.c utils/sintab.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/dac.doj: drivers/dac.c drivers/dac.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/eeprom.doj: drivers/eeprom.c drivers/eeprom.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/flash.doj: drivers/flash.c drivers/flash.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/${PROGNAME}_basiccrt.doj: system/${PROGNAME}_basiccrt.s
	${AS} system/${PROGNAME}_basiccrt.s ${ASFLAG} -o ${RELEASE_FOLDER}/${PROGNAME}_basiccrt.doj 

${RELEASE_FOLDER}/${PROGNAME}_heaptab.doj: system/${PROGNAME}_heaptab.c
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/gps.doj: drivers/gps.c drivers/gps.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/irq.doj: irq/irq.c irq/irq.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/led.doj: drivers/led.c drivers/led.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/log.doj: log/log.c log/log.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/lsm303.doj: drivers/lsm303.c drivers/lsm303.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/modem.doj: modem/modem.c modem/modem.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/pll.doj: pll/pll.c pll/pll.h globdefs.h config.h version.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/ports.doj: drivers/ports.c drivers/ports.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/power.doj: drivers/power.c drivers/power.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/rele.doj: drivers/rele.c drivers/rele.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/rsi.doj: periph/rsi.c periph/rsi.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/spi0.doj: periph/spi0.c periph/spi0.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/spi1.doj: periph/spi1.c periph/spi1.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/sport0.doj: periph/sport0.c periph/sport0.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/timer0.doj: periph/timer0.c periph/timer0.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/timer1.doj: periph/timer1.c periph/timer1.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/timer2.doj: periph/timer2.c periph/timer2.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/timer3.doj: periph/timer3.c periph/timer3.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/timer4.doj: periph/timer4.c periph/timer4.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/twi.doj: periph/twi.c periph/twi.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/uart0.doj: periph/uart0.c periph/uart0.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/uart1.doj: periph/uart1.c periph/uart1.h globdefs.h config.h version.h  pll/pll.h
	${CC} ${CFLAGS} -c $^ -o $@

${RELEASE_FOLDER}/main.doj: main.c main.h globdefs.h version.h  pll/pll.h pll\pll.h version.h  pll/pll.h

	@echo ".\main.c"
	${CC} ${CFLAGS} -c main.c  -o ${RELEASE_FOLDER}/main.doj


.PHONY : clean
