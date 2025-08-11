# GBDK example for the Mega Duck Laptop
How to interface with the printer made for the Mega Duck Laptop
models ("Super QuiQue" and "Super Junior Computer").

## Purpose
This repo is a staging ground for printer protocol research and the
subsequent example that will (likely) be merged into GBDK.

## Mega Duck Printer example
- Initializing the external controller connected over the serial link port
- Checking whether the printer is connected and which type
- Printing the screen to the printer

## Printer Types
There are two types of Mega Duck printer, single pass and double pass.

- The single pass monochrome model is the standard Mega Duck JC-510 thermal printer which connects to the DB-26 "Printer" port.
- The double pass model 3-color model (identified in the System ROM disassembly) is most likely the JC-502 "Drucker Interface" which connects to DB-15 "External Connector" port and allows interfacing with standard (1990's) parallel printers.

Printing example support for the double pass model is untested
on hardware at present and so not included in the `main` branch.
For it's example code see the `save_2_pass_printer` branch.
