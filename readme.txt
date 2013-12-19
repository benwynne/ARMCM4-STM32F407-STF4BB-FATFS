*****************************************************************************
** ChibiOS/RT port for ARM-Cortex-M4 STM32F407.                            **
*****************************************************************************

** TARGET **

The demo runs on a STM32F4-Discovery board + Embest DM-STF4BB
    www.embest-tech.com/product/extension-module/dm-stf4bb-base-board.html
    
** The Demo **

The MicroSD port on the Embest board is used for FATfs actions.

The USB-FS port is used as USB-CDC and a command shell is ready to accepts
commands there.
    tree
        Display file tree
    free
        Display amount of free space in B, KB and MB
    hello
        Creates hello.txt with 'Hello World' on the base level of the MicroSD Card.
    mkdir [dir]
        Makes directory dir on the base level of the MicroSD Card.
    cat [file]
        Echos a file from the MicroSD card to the console

** Build Procedure **

The demo has been tested by using the free GCC-based toolchain included with 
ChibiStudio.

Just modify the TRGT line in the makefile in order to use different GCC ports.

** Notes **

Some files used by the demo are not part of ChibiOS/RT but are copyright of
ST Microelectronics and are licensed under a different license.
Also note that not all the files present in the ST library are distributed
with ChibiOS/RT, you can find the whole library on the ST web site:

                             http://www.st.com
