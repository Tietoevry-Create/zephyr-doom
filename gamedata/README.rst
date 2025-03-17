##############################
Zephyr Doom - Game Data
##############################

QSPI Flash Upload
====================

1. Ensure you have the Nordic command line tools installed (nrfjprog)
2. Connect your board via USB
3. Execute the following command:

.. code-block:: console

    nrfjprog --family nrf53 --qspicustominit --qspieraseall && nrfjprog --family nrf53 --qspicustominit --program qspi.hex --verify

This will first erase the external QSPI flash and then program it with the data in the qspi.hex file.


Custom WAD Files
==================

If you want to play with modified DOOM WAD files:

1. Format an SD card to FAT32
2. Copy your custom WAD file to the root directory of the SD card
3. Insert the SD card into an SD card slot connected to the board
4. Hold down button 2 while restarting the board, which will cause the game to load the WAD file and write it to the external QSPI flash
5. The game will now use your custom WAD file


The qspi.hex file in this folder was created by:

1. Downloading the DOOM shareware v1.9 WAD file
2. Following the steps above to upload the WAD file to the external QSPI flash
3. Using the following command to read the contents of the QSPI flash and save it as qspi.hex:

.. code-block:: console

    nrfjprog --family nrf53 --readqspi qspi.hex
