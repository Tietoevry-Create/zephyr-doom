##############################
Zephyr Doom - Game Data
##############################

Version Notice
====================
This README is valid for the code from commit 4e4f9a4 (MVP1) and requires the Arduino 3.5" TFT LCD Shield.

Shield Pin Mapping
====================
| Peripheral  | Function | nRF5340 Pin   |
|-------------|----------|---------------|
| SPI/SD card | SS       | P1.12         |
| SPI/SD card | DI       | P1.13         |
| SPI/SD card | DO       | P1.14         |
| SPI/SD card | SCK      | P1.15         |
| LCD         | Bit 1-2  | P1.10 - P1.11 |
| LCD         | Bit 3-8  | P1.04 - P1.09 |
| LCD         | RST      | P0.25         |
| LCD         | CS       | P0.07         |
| LCD         | RS       | P0.06         |
| LCD         | WR       | P0.05         |
| LCD         | RD       | P0.04         |


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

1. Downloading `the DOOM shareware v1.9 WAD file <https://archive.org/details/DoomsharewareEpisode>`_.
2. Following the steps above to upload the WAD file to the external QSPI flash
3. Using the following command to read the contents of the QSPI flash and save it as qspi.hex:

.. code-block:: console

    nrfjprog --family nrf53 --readqspi qspi.hex

Editing WAD Files
==================

Here's how to edit and use your own WAD file:

1. **Choose a WAD Editor**

  Popular options include:

  * `SLADE3 <https://slade.mancubus.net/>`_: A versatile editor that handles graphics, audio, textures, and map editing.
  * `Ultimate Doom Builder <https://github.com/jewalky/UltimateDoomBuilder>`_: Primarily focused on creating and editing maps.

2. **Edit the WAD file**

  Open the WAD file using your chosen editor. You can:

  * Replace textures or sprites.
  * Edit or create new maps.
  * Change sound effects or music.

3. **Save your custom WAD**

  Save your changes as a new WAD file (e.g., ``MYDOOM.WAD``).

4. **Load your custom WAD onto the board**

  Follow the steps outlined in the **Custom WAD Files** section above to transfer your edited WAD onto the board's external QSPI flash.

**Tips:**
  * Always back up the original WAD file before editing.
  * Ensure your custom WAD file stays within the size limit supported by the external flash (8MiB).
  * Test your custom WAD thoroughly on a desktop DOOM port (like Chocolate Doom) before deploying it to your board, making debugging easier.