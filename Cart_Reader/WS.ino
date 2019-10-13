//******************************************
// WonderSwan MODULE
//******************************************
// WonderSwan cartridge pinout
// 48P 1.25mm pitch connector
// C1, C48     : GND
// C24, C25    : VDD (+3.3v)
// C16-C23     : D7-D0
// C34-C39     : D8-D13
// C14-C15     : D15-D14
// C26-C29     : A(-1)-A2
// C10-C13     : A6-A3
// C30-C33     : A18-A15
// C2,C3,C4,C5 : A14,A9,A10,A8
// C6,C7,C8,C9 : A7,A12,A13,A11
// C40         : /RST
// C41         : /IO? (only use when unlocking MMC)
// C42         : /MMC (access port on cartridge with both /CART and /MMC = L)
// C43         : /OE
// C44         : /WE
// C45         : /CART? (L when accessing cartridge (ROM/SRAM/PORT))
// C46         : INT (for RTC alarm interrupt)
// C47         : CLK (384KHz on wonderswan)
// ------------------------------------------
// how to connect to CartReader
// ------------------------------------------
// /RST   : PH0
// /CART? : PH3
// /MMC   : PH4
// /WE    : PH5
// /OE    : PH6
// CLK    : PE3
// /IO?   : PE4 
// INT    : PG5
// A(-1)-A6: PF0-PF7
// A7-A14:  PK0-PK7
// A15-A18: PL0-PL3
// D0-D7  : PC0-PC7
// D8-D15 : PA0-PA7

/******************************************
  Menu
*****************************************/
static const char wsMenuItem1[] PROGMEM = "Read Rom";
static const char wsMenuItem2[] PROGMEM = "Read Save";
static const char wsMenuItem3[] PROGMEM = "Write Save";
static const char wsMenuItemReset[] PROGMEM = "Reset";
static const char* const menuOptionsWS[] PROGMEM = {wsMenuItem1, wsMenuItem2, wsMenuItem3, wsMenuItemReset};

/******************************************
 * Developer Name
*****************************************/
static const char wsDevNameXX[] PROGMEM = "XXX";
static const char wsDevName01[] PROGMEM = "BAN";
static const char wsDevName12[] PROGMEM = "KNM";
static const char wsDevName18[] PROGMEM = "KGT";
static const char wsDevName1D[] PROGMEM = "BEC";
static const char wsDevName24[] PROGMEM = "0MN";
static const char wsDevName28[] PROGMEM = "SQR";
static const char wsDevName31[] PROGMEM = "VGD";
static const char wsDevName7A[] PROGMEM = "7AC";
static const char wsDevNameFF[] PROGMEM = "WWGP";

static uint8_t wsGameOrientation = 0;
static uint8_t wsGameHasRTC = 0;
static uint16_t wsGameChecksum = 0;
static uint8_t wsEepromShiftReg[2];

void setup_WS()
{
  // A0 - A7
  DDRF = 0xff;
  // A8 - A15
  DDRK = 0xff;
  // A16 - A23
  DDRL = 0xff;

  // D0 - D15
  DDRC = 0x00;
  DDRA = 0x00;

  // controls
  DDRH |= ((1 << 0) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));
  PORTH |= ((1 << 0) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));
  
  DDRE |= ((1 << 3) | (1 << 4));
  PORTE |= (1 << 4);
  PORTE &= ~(1 << 3);

  // interrupt pin with internal pull-up
  DDRG &= ~(1 << 5);
  PORTG |= (1 << 5);

  display_Clear();
  
  // unlock MMC
  if (!unlockMMC2003_WS())
    print_Error(F("Can't initial MMC"), true);

  if (getCartInfo_WS() != 0xea)
    print_Error(F("Rom header read error"), true);
  
  showCartInfo_WS();
  mode = mode_WS;
}

void wsMenu()
{
  uint8_t mainMenu;

  convertPgm(menuOptionsWS, 6);
  mainMenu = question_box(F("WonderSwan Menu"), menuOptions, 6, 0);

  switch (mainMenu)
  {
    case 0:
    {
      // Read Rom
      sd.chdir("/");
      readROM_WS(filePath, FILEPATH_LENGTH);
      sd.chdir("/");
      compareChecksum_WS(filePath);
      break;
    }
    case 1:
    {
      // Read Save
      sd.chdir("/");
      switch (saveType)
      {
        case 0: println_Msg(F("No save for this game")); break;
        case 1: readSRAM_WS(); break;
        case 2: readEEPROM_WS(); break;
        default: println_Msg(F("Unknow save type")); break;
      }
      
      break;
    }
    case 2:
    {
      // Write Save
      sd.chdir("/");
      switch (saveType)
      {
        case 0: println_Msg(F("No save for this game")); break;
        case 1:
        {
          writeSRAM_WS();
          verifySRAM_WS(); 
          break;
        }
        case 2:
        {
          writeEEPROM_WS();
          verifyEEPROM_WS();
          break;
        }
        default: println_Msg(F("Unknow save type")); break;
      }
      
      break;
    }
    default:
    {
      asm volatile ("  jmp 0");
      break;
    }
  }
  
  println_Msg(F(""));
  println_Msg(F("Press Button..."));  
  
  display_Update();
  wait(); 
}

uint8_t getCartInfo_WS()
{
  dataIn_WS();

  for (uint32_t i = 0; i < 16; i += 2)
    *((uint16_t*)(sdBuffer + i)) = readWord_WS(0xffff0 + i);

  wsGameChecksum = *(uint16_t*)(sdBuffer + 14);

  // some game has wrong info in header
  // patch here
  switch (wsGameChecksum)
  {
    // games with 256kbits SRAM installed
    case 0xe600:  // BAN007
    case 0x8eed:  // BANC16
    {
      sdBuffer[11] = 0x02;
      break;
    }
    // games missing 'COLOR' flag
    case 0x26db:  // SQRC01
    {
      sdBuffer[7] = 0x01;
      break;
    }
    case 0x7f73:  // BAN030
    {
      // missing developerId and cartId
      sdBuffer[6] = 0x01;
      sdBuffer[8] = 0x30;
      break;
    }
    case 0x0000:
    {
      // developerId/cartId/checksum are all filled with 0x00 in wonderwitch based games
      if (readWord_WS(0xf0000) == 0x4c45 && readWord_WS(0xf0002) == 0x5349 && readWord_WS(0xf0004) == 0x0041)
      {
        // check wonderwitch BIOS
        if (readWord_WS(0xfff5e) == 0x006c && readWord_WS(0xfff60) == 0x5b1b)
        {
          // wonderwitch SWJ-7AC003
          sdBuffer[6] = 0x7a;
          sdBuffer[8] = 0x03;
          // TODO check OS version and fill into version field
        }
        // check service menu
        else if (readWord_WS(0xfff22) == 0x006c && readWord_WS(0xfff24) == 0x5b1b)
        {
          if (readWord_WS(0x93246) == 0x4a2f && readWord_WS(0x93248) == 0x5353 && readWord_WS(0x9324a) == 0x2e32)
          {
            // jss2
            sdBuffer[6] = 0xff; // WWGP
            sdBuffer[8] = 0x1a; // 2001A
            sdBuffer[7] = 0x01; // color only

            if (readWord_WS(0x93e9c) == 0x4648 && readWord_WS(0x93e9e) == 0x0050)
            {
              // WWGP2001A3 -> HFP Version
              sdBuffer[9] = 0x03;
              wsGameChecksum = 0x4870;              
            }
            else
            {
              // TODO check other jss2 version
            }          
          }
          else if (readWord_WS(0xe4260) == 0x6b64 && readWord_WS(0xe4262) == 0x696e)
          {
            // dknight
            sdBuffer[6] = 0xff; // WWGP
            sdBuffer[8] = 0x2b; // 2002B
            sdBuffer[7] = 0x01; // color only
            sdBuffer[9] = 0x00;
            wsGameChecksum = 0x8b1c;
          }
        }
      }
      break;
    }
  }
  
  romType = sdBuffer[7]; // wsc only = 1
  romVersion = sdBuffer[9];
  romSize = sdBuffer[10];
  sramSize = sdBuffer[11];
  wsGameOrientation = (sdBuffer[12] & 0x01);
  wsGameHasRTC = (sdBuffer[13] & 0x01);

  snprintf(vendorID, 5, "%02X", sdBuffer[6]);
  snprintf(cartID, 5, "%c%02X", ((romType & 0x01) ? 'C' : '0'), sdBuffer[8]);
  snprintf(checksumStr, 5, "%04X", wsGameChecksum);
  snprintf(romName, 17, "%s%s", vendorID, cartID);

  switch (romSize)
  {
    case 0x01: cartSize = 131072 * 2; break;
    case 0x02: cartSize = 131072 * 4; break;
    case 0x03: cartSize = 131072 * 8; break;
    case 0x04: cartSize = 131072 * 16; break;
    // case 0x05: cartSize = 131072 * 24; break;
    case 0x06: cartSize = 131072 * 32; break;
    // case 0x07: cartSize = 131072 * 48; break;
    case 0x08: cartSize = 131072 * 64; break;
    case 0x09: cartSize = 131072 * 128; break;
    default: cartSize = 0; break;
  }
  
  switch (sramSize)
  {
    case 0x00: saveType = 0; sramSize = 0; break;
    case 0x01: saveType = 1; sramSize = 64; break;
    case 0x02: saveType = 1; sramSize = 256; break;
    case 0x03: saveType = 1; sramSize = 1024; break;
    case 0x04: saveType = 1; sramSize = 2048; break;
    case 0x10: saveType = 2; sramSize = 1; break;
    case 0x20: saveType = 2; sramSize = 16; break;
    case 0x50: saveType = 2; sramSize = 8; break;
    default: saveType = 0xff; break;
  }

  if (saveType == 2)
    unprotectEEPROM();

  // should be 0xea (JMPF instruction)
  return sdBuffer[0];
}

void showCartInfo_WS()
{
  display_Clear();

  println_Msg(F("WonderSwan Cart Info"));
  
  print_Msg(F("Game: "));
  println_Msg(romName);

  print_Msg(F("Rom Size: "));
  if (cartSize == 0x00)
    println_Msg(romSize, HEX);
  else
  {
    print_Msg(cartSize / 131072);
    println_Msg(F(" Mb"));
  }

  print_Msg(F("Save: "));
  switch (saveType)
  {
    case 0: println_Msg(F("None")); break;
    case 1: print_Msg(F("Sram ")); print_Msg(sramSize); println_Msg(F("Kb")); break;
    case 2: print_Msg(F("Eeprom ")); print_Msg(sramSize); println_Msg(F("Kb")); break;
    default: println_Msg(sramSize, HEX); break;
  }

  print_Msg(F("Vesion: 1."));
  println_Msg(romVersion, HEX);

  print_Msg(F("Checksum: "));
  println_Msg(checksumStr);
  
  println_Msg(F("Press Button..."));  
  display_Update();
  wait();
}

void readROM_WS(char *outPathBuf, size_t bufferSize)
{
  // generate fullname of rom file
  snprintf(fileName, FILENAME_LENGTH, "%s.ws", romName);

  // create a new folder for storing rom file
  EEPROM_readAnything(0, foldern);
  snprintf(folder, sizeof(folder), "WS/ROM/%s/%d", romName, foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  // filling output file path to buffer
  if (outPathBuf != NULL && bufferSize > 0)
    snprintf(outPathBuf, bufferSize, "%s/%s", folder, fileName);

  display_Clear();
  print_Msg(F("Saving to "));
  print_Msg(folder);
  println_Msg(F("/..."));
  display_Update();

  // open file on sdcard
  if (!myFile.open(fileName, O_RDWR | O_CREAT))
    print_Error(F("Can't create file on SD"), true);

  // write new folder number back to EEPROM
  foldern++;
  EEPROM_writeAnything(0, foldern);

  // get correct starting rom bank
  uint16_t bank = (256 - (cartSize >> 16));

  // start reading rom
  for (; bank <= 0xff; bank++)
  {
    // switch bank on segment 0x2
    dataOut_WS();
    writeByte_WSPort(0xc2, bank);

    dataIn_WS();
    for (uint32_t addr = 0; addr < 0x10000; addr += 512)
    {
      // blink LED
      if ((addr & ((1 << 14) - 1)) == 0)
        PORTB ^= (1 << 4);

       for (uint32_t w = 0; w < 512; w += 2)
         *((uint16_t*)(sdBuffer + w)) = readWord_WS(0x20000 + addr + w);

       myFile.write(sdBuffer, 512);
    }
  }

  myFile.close();
}

void readSRAM_WS()
{
  // generate fullname of rom file
  snprintf(fileName, FILENAME_LENGTH, "%s.sav", romName);

  // create a new folder for storing rom file
  EEPROM_readAnything(0, foldern);
  snprintf(folder, sizeof(folder), "WS/SAVE/%s/%d", romName, foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  display_Clear();
  print_Msg(F("Saving "));
  print_Msg(folder);
  println_Msg(F("/..."));
  display_Update();

  foldern++;
  EEPROM_writeAnything(0, foldern);

  if (!myFile.open(fileName, O_RDWR | O_CREAT))
    print_Error(F("Can't create file on SD"), true);

  uint32_t bank_size = (sramSize << 7);
  uint16_t end_bank = (bank_size >> 16);  // 64KB per bank

  if (bank_size > 0x10000)
      bank_size = 0x10000;
  
  uint16_t bank = 0;

  do
  {
    dataOut_WS();
    writeByte_WSPort(0xc1, bank);

    dataIn_WS();
    for (uint32_t addr = 0; addr < bank_size; addr += 512)
    {
      // blink LED
      if ((addr & ((1 << 14) - 1)) == 0)
        PORTB ^= (1 << 4);

      // SRAM data on D0-D7, with A-1 to select high/low byte
      for (uint32_t w = 0; w < 512; w++)
        sdBuffer[w] = readByte_WS(0x10000 + addr + w);

       myFile.write(sdBuffer, 512);
    }
  } while (++bank < end_bank);

  myFile.close();

  println_Msg(F("Done"));
  display_Update();
}

void verifySRAM_WS()
{
  print_Msg(F("Verifying... "));
  display_Update();
  
  if (myFile.open(filePath, O_READ))
  {
    uint32_t bank_size = (sramSize << 7);
    uint16_t end_bank = (bank_size >> 16);  // 64KB per bank
    uint16_t bank = 0;
    uint32_t write_errors = 0;
    
    if (bank_size > 0x10000)
       bank_size = 0x10000;

    do
    {
      dataOut_WS();
      writeByte_WSPort(0xc1, bank);

      dataIn_WS();
      for (uint32_t addr = 0; addr < bank_size && myFile.available(); addr += 512)
      {
        myFile.read(sdBuffer, 512);

        // SRAM data on D0-D7, with A-1 to select high/low byte
        for (uint32_t w = 0; w < 512; w++)
        {
          if (readByte_WS(0x10000 + addr + w) != sdBuffer[w])
            write_errors++;
        }
      }
    } while (++bank < end_bank);

    myFile.close();

    if (write_errors == 0)
    {
      println_Msg(F("passed"));
    }
    else
    {
      println_Msg(F("failed"));
      print_Msg(F("Error: "));
      print_Msg(write_errors);
      println_Msg(F(" bytes "));
      print_Error(F("did not verify."), false);
    }
  }
  else
  {
    print_Error(F("File doesn't exist"), false);
  }
}

void writeSRAM_WS()
{
  filePath[0] = 0;
  sd.chdir("/");
  fileBrowser(F("Select sav file"));
  snprintf(filePath, FILEPATH_LENGTH, "%s/%s", filePath, fileName);

  display_Clear();
  print_Msg(F("Writing "));
  print_Msg(filePath);
  println_Msg(F("..."));
  display_Update();

  if (myFile.open(filePath, O_READ))
  {
    uint32_t bank_size = (sramSize << 7);
    uint16_t end_bank = (bank_size >> 16);  // 64KB per bank

    if (bank_size > 0x10000)
        bank_size = 0x10000;
  
    uint16_t bank = 0;
    dataOut_WS();
    do
    {
      writeByte_WSPort(0xc1, bank);

      for (uint32_t addr = 0; addr < bank_size && myFile.available(); addr += 512)
      {
        // blink LED
        if ((addr & ((1 << 14) - 1)) == 0)
          PORTB ^= (1 << 4);

        myFile.read(sdBuffer, 512);

        // SRAM data on D0-D7, with A-1 to select high/low byte
        for (uint32_t w = 0; w < 512; w++)
          writeByte_WS(0x10000 + addr + w, sdBuffer[w]);
      }
    } while (++bank < end_bank);

    myFile.close();

    println_Msg(F("Writing finished"));
    display_Update();
  }
  else
  {
    print_Error(F("File doesn't exist"), false);
  }
}

void readEEPROM_WS()
{
  // generate fullname of eep file
  snprintf(fileName, FILENAME_LENGTH, "%s.eep", romName);

  // create a new folder for storing eep file
  EEPROM_readAnything(0, foldern);
  snprintf(folder, sizeof(folder), "WS/SAVE/%s/%d", romName, foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  display_Clear();
  print_Msg(F("Saving "));
  print_Msg(folder);
  println_Msg(F("/..."));
  display_Update();

  foldern++;
  EEPROM_writeAnything(0, foldern);

  if (!myFile.open(fileName, O_RDWR | O_CREAT))
    print_Error(F("Can't create file on SD"), true);

  uint32_t eepromSize = (sramSize << 7);
  uint32_t bufSize = (eepromSize < 512 ? eepromSize : 512);
  
  for (uint32_t i = 0; i < eepromSize; i += bufSize)
  {
    for (uint32_t j = 0; j < bufSize; j += 2)
    {
      // blink LED
      if ((j & 0x1f) == 0x00)
        PORTB ^= (1 << 4);
      
      generateEepromInstruction_WS(wsEepromShiftReg, 0x2, ((i + j) >> 1));

      dataOut_WS();
      writeByte_WSPort(0xc6, wsEepromShiftReg[0]);
      writeByte_WSPort(0xc7, wsEepromShiftReg[1]);
      writeByte_WSPort(0xc8, 0x10);

      // MMC will shift out from port 0xc7 to 0xc6
      // and shift in 16bits into port 0xc5 to 0xc4
      pulseCLK_WS(1 + 32 + 3);

      dataIn_WS();
      sdBuffer[j] = readByte_WSPort(0xc4);
      sdBuffer[j + 1] = readByte_WSPort(0xc5);
    }

    myFile.write(sdBuffer, bufSize);
  }

  myFile.close();

  println_Msg(F("Done"));
}

void verifyEEPROM_WS()
{
  print_Msg(F("Verifying... "));
  display_Update();
    
  if (myFile.open(filePath, O_READ))
  {
    uint32_t write_errors = 0;
    uint32_t eepromSize = (sramSize << 7);
    uint32_t bufSize = (eepromSize < 512 ? eepromSize : 512);
    
    for (uint32_t i = 0; i < eepromSize; i += bufSize)
    {
      myFile.read(sdBuffer, bufSize);

      for (uint32_t j = 0; j < bufSize; j += 2)
      {
        // blink LED
        if ((j & 0x1f) == 0x00)
          PORTB ^= (1 << 4);
      
        generateEepromInstruction_WS(wsEepromShiftReg, 0x2, ((i + j) >> 1));

        dataOut_WS();
        writeByte_WSPort(0xc6, wsEepromShiftReg[0]);
        writeByte_WSPort(0xc7, wsEepromShiftReg[1]);
        writeByte_WSPort(0xc8, 0x10);

        // MMC will shift out from port 0xc7 to 0xc6
        // and shift in 16bits into port 0xc5 to 0xc4
        pulseCLK_WS(1 + 32 + 3);

        dataIn_WS();
        if (readByte_WSPort(0xc4) != sdBuffer[j])
          write_errors++;

        if (readByte_WSPort(0xc5) != sdBuffer[j + 1])
          write_errors++;        
      }
    }

    myFile.close();

    if (write_errors == 0)
    {
      println_Msg(F("passed"));
    }
    else
    {
      println_Msg(F("failed"));
      print_Msg(F("Error: "));
      print_Msg(write_errors);
      println_Msg(F(" bytes "));
      print_Error(F("did not verify."), false);
    }    
  }
  else
  {
    print_Error(F("File doesn't exist"), false);
  }
}

void writeEEPROM_WS()
{
  filePath[0] = 0;
  sd.chdir("/");
  fileBrowser(F("Select eep file"));
  snprintf(filePath, FILEPATH_LENGTH, "%s/%s", filePath, fileName);

  display_Clear();
  print_Msg(F("Writing "));
  print_Msg(filePath);
  println_Msg(F("..."));
  display_Update();

  if (myFile.open(filePath, O_READ))
  {
    uint32_t eepromSize = (sramSize << 7);
    uint32_t bufSize = (eepromSize < 512 ? eepromSize : 512);
    
    for (uint32_t i = 0; i < eepromSize; i += bufSize)
    {
      myFile.read(sdBuffer, bufSize);
        
      for (uint32_t j = 0; j < bufSize; j += 2)
      {
        // blink LED
        if ((j & 0x1f) == 0x00)
          PORTB ^= (1 << 4);

        generateEepromInstruction_WS(wsEepromShiftReg, 0x1, ((i + j) >> 1));
        
        dataOut_WS();
        writeByte_WSPort(0xc6, wsEepromShiftReg[0]);
        writeByte_WSPort(0xc7, wsEepromShiftReg[1]);
        writeByte_WSPort(0xc4, sdBuffer[j]);
        writeByte_WSPort(0xc5, sdBuffer[j + 1]);
        writeByte_WSPort(0xc8, 0x20);

        // MMC will shift out from port 0xc7 to 0xc4
        pulseCLK_WS(1 + 32 + 3);

        dataIn_WS();
        do { pulseCLK_WS(128); }
        while ((readByte_WSPort(0xc8) & 0x02) == 0x00);
      }
    }

    myFile.close();

    println_Msg(F("Done"));
  }
  else
  {
    print_Error(F("File doesn't exist"), false);
  }
}

boolean compareChecksum_WS(const char *wsFilePath)
{
  if (wsFilePath == NULL)
    return 0;
  
  println_Msg(F("Calculating Checksum"));
  display_Update();

  if (!myFile.open(wsFilePath, O_READ))
  {
    print_Error(F("Failed to open file"), false);
    return 0;
  }

  uint32_t calLength = myFile.fileSize() - 512;
  uint32_t checksum = 0;

  if (strncmp(romName, "7A003", 6) == 0)
  {
    // only calcuate last 128Kbytes for wonderwitch (OS and BIOS region)
    myFile.seekCur(myFile.fileSize() - 131072);
    calLength = 131072 - 512;
  }
  
  for (uint32_t i = 0; i < calLength; i += 512)
  {
    myFile.read(sdBuffer, 512);
    for (uint32_t j = 0; j < 512; j++)
      checksum += sdBuffer[j];
  }

  // last 512 bytes
  myFile.read(sdBuffer, 512);
  // skip last 2 bytes (checksum value)
  for (uint32_t j = 0; j < 510; j++)
    checksum += sdBuffer[j];

  myFile.close();

  checksum &= 0x0000ffff;
  calLength = wsGameChecksum;

  // don't know why formating string "%04X(%04X)" always output "xxxx(0000)"
  // so split into two snprintf 
  char result[11];
  snprintf(result, 5, "%04X", calLength);
  snprintf(result + 4, 11 - 4, "(%04X)", checksum);
  print_Msg(F("Result: "));
  println_Msg(result);

  if (checksum == calLength)
  {
    println_Msg(F("Checksum matches"));
    display_Update();
    return 1;
  }
  else
  {
    print_Error(F("Checksum Error"), false);
    return 0;
  }
}

void writeByte_WSPort(uint8_t port, uint8_t data)
{
  PORTF = (port & 0x0f);
  PORTL = (port >> 4);

  // switch CART(PH3), MMC(PH4) to LOW
  PORTH &= ~((1 << 3) | (1 << 4));

  // set data
  PORTC = data;

  // switch WE(PH5) to LOW
  PORTH &= ~(1 << 5);
  NOP;

  // switch WE(PH5) to HIGH
  PORTH |= (1 << 5);
  NOP; NOP;
  
  // switch CART(PH3), MMC(PH4) to HIGH
  PORTH |= ((1 << 3) | (1 << 4));
}

uint8_t readByte_WSPort(uint8_t port)
{
  PORTF = (port & 0x0f);
  PORTL = (port >> 4);

  // switch CART(PH3), MMC(PH4) to LOW
  PORTH &= ~((1 << 3) | (1 << 4));

  // switch OE(PH6) to LOW
  PORTH &= ~(1 << 6);
  NOP; NOP; NOP;

  uint8_t ret = PINC;

  // switch OE(PH6) to HIGH
  PORTH |= (1 << 6);
  
  // switch CART(PH3), MMC(PH4) to HIGH
  PORTH |= ((1 << 3) | (1 << 4));

  return ret;
}

void writeWord_WS(uint32_t addr, uint16_t data)
{
  PORTF = addr & 0xff;
  PORTK = (addr >> 8) & 0xff;
  PORTL = (addr >> 16) & 0x0f;

  PORTC = data & 0xff;
  PORTA = (data >> 8);
  
  // switch CART(PH3) and WE(PH5) to LOW
  PORTH &= ~((1 << 3) | (1 << 5));
  NOP;

  // switch CART(PH3) and WE(PH5) to HIGH
  PORTH |= (1 << 3) | (1 << 5);
  NOP; NOP;
}

uint16_t readWord_WS(uint32_t addr)
{
  PORTF = addr & 0xff;
  PORTK = (addr >> 8) & 0xff;
  PORTL = (addr >> 16) & 0x0f;
  
  // switch CART(PH3) and OE(PH6) to LOW
  PORTH &= ~((1 << 3) | (1 << 6));
  NOP; NOP; NOP;

  uint16_t ret = ((PINA << 8) | PINC);

  // switch CART(PH3) and OE(PH6) to HIGH
  PORTH |= (1 << 3) | (1 << 6);

  return ret;  
}

void writeByte_WS(uint32_t addr, uint8_t data)
{
  PORTF = addr & 0xff;
  PORTK = (addr >> 8) & 0xff;
  PORTL = (addr >> 16) & 0x0f;

  PORTC = data;
  
  // switch CART(PH3) and WE(PH5) to LOW
  PORTH &= ~((1 << 3) | (1 << 5));
  NOP;

  // switch CART(PH3) and WE(PH5) to HIGH
  PORTH |= (1 << 3) | (1 << 5);
  NOP; NOP;
}

uint8_t readByte_WS(uint32_t addr)
{
  PORTF = addr & 0xff;
  PORTK = (addr >> 8) & 0xff;
  PORTL = (addr >> 16) & 0x0f;
  
  // switch CART(PH3) and OE(PH6) to LOW
  PORTH &= ~((1 << 3) | (1 << 6));
  NOP; NOP; NOP;

  uint8_t ret = PINC;

  // switch CART(PH3) and OE(PH6) to HIGH
  PORTH |= (1 << 3) | (1 << 6);

  return ret;  
}

void unprotectEEPROM()
{
  generateEepromInstruction_WS(wsEepromShiftReg, 0x0, 0x3);
  
  dataOut_WS();
  writeByte_WSPort(0xc6, wsEepromShiftReg[0]);
  writeByte_WSPort(0xc7, wsEepromShiftReg[1]);
  writeByte_WSPort(0xc8, 0x40);

  // MMC will shift out port 0xc7 to 0xc6 to EEPROM
  pulseCLK_WS(1 + 16 + 3);
}

// generate data for port 0xc6 to 0xc7
// number of CLK pulses needed for each instruction is 1 + (16 or 32) + 3
void generateEepromInstruction_WS(uint8_t *instruction, uint8_t opcode, uint16_t addr)
{
  uint8_t addr_bits = (sramSize > 1 ? 10 : 6);
  uint16_t *ptr = (uint16_t*)instruction;
  *ptr = 0x0001; // initial with a start bit

  if (opcode == 0)
  {
    // 2bits opcode = 0x00
    *ptr <<= 2;
    // 2bits ext cmd (from addr)
    *ptr <<= 2;
    *ptr |= (addr & 0x0003);
    *ptr <<= (addr_bits - 2);    
  }
  else
  {
    // 2bits opcode
    *ptr <<= 2;
    *ptr |= (opcode & 0x03);
    // address bits
    *ptr <<= addr_bits;
    *ptr |= (addr & ((1 << addr_bits) - 1));
  }  
}

// 2003 MMC need to be unlock,
// or it will reject all reading and bank switching
boolean unlockMMC2003_WS()
{
  dataOut_WS();

  // initialize all control pin state
  // RST(PH0) and CLK(PE3) to LOW
  // CART(PH3) MMC(PH4) WE(PH5) OE(PH6) IO(PE4) to HIGH
  PORTH &= ~(1 << 0);
  PORTE &= ~(1 << 3);
  PORTH |= ((1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));
  PORTE |= (1 << 4);
  
  // switch RST(PH0) to HIGH
  PORTH |= (1 << 0);

  // data = 0x00ff
  PORTC = 0xff;
  PORTA = 0x00;  

  // port = 0x5a?
  PORTF = 0x0a;
  PORTL = 0x05;
  pulseCLK_WS(5);

  // port = 0xa5?
  PORTF = 0x05;
  PORTL = 0x0a;
  pulseCLK_WS(4);

   // IO(PE4) to LOW
  PORTE &= ~(1 << 4);
  pulseCLK_WS(6);

  // IO(PE4) to HIGH
  PORTE |= (1 << 4);
  pulseCLK_WS(1);

  // IO(PE4) to LOW
  PORTE &= ~(1 << 4);
  pulseCLK_WS(1);

  // IO(PE4) to HIGH
  PORTE |= (1 << 4);
  pulseCLK_WS(1);

  // IO(PE4) to LOW
  PORTE &= ~(1 << 4);
  pulseCLK_WS(3);

  // IO(PE4) to HIGH
  PORTE |= (1 << 4);
  pulseCLK_WS(1);

  // IO(PE4) to LOW
  PORTE &= ~(1 << 4);
  pulseCLK_WS(1);

  // IO(PE4) to HIGH
  PORTE |= (1 << 4);
  pulseCLK_WS(1);

  // IO(PE4) to LOW
  PORTE &= ~(1 << 4);
  pulseCLK_WS(3);

  // IO(PE4) to HIGH
  PORTE |= (1 << 4);

  // pulse CLK once
  pulseCLK_WS(1);

  // unlock procedure finished
  // see if we can set bank number to MMC
  writeByte_WSPort(0xc2, 0xaa);
  writeByte_WSPort(0xc3, 0x55);

  dataIn_WS();
  if (readByte_WSPort(0xc2) == 0xaa && readByte_WSPort(0xc3) == 0x55)
  {
    // now set initial bank number to MMC
    dataOut_WS();
    writeByte_WSPort(0xc0, 0x2f);
    writeByte_WSPort(0xc1, 0x3f);
    writeByte_WSPort(0xc2, 0xff);
    writeByte_WSPort(0xc3, 0xff);
    return true;
  }

  return false;
}

// doing a L->H on CLK(PE3) pin
void pulseCLK_WS(uint8_t count)
{
  register uint8_t tic;
  
  // about 384KHz, 50% duty cycle
  asm volatile
  ("L0_%=:\n\t"
   "cpi %[count], 0\n\t"
   "breq L3_%=\n\t"
   "dec %[count]\n\t"
   "cbi %[porte], 3\n\t"
   "ldi %[tic], 6\n\t"
   "L1_%=:\n\t"
   "dec %[tic]\n\t"
   "brne L1_%=\n\t"
   "sbi %[porte], 3\n\t"
   "ldi %[tic], 5\n\t"
   "L2_%=:\n\t"
   "dec %[tic]\n\t"
   "brne L2_%=\n\t"
   "rjmp L0_%=\n\t"
   "L3_%=:\n\t"
   : [tic] "=a" (tic)
   : [count] "a" (count), [porte] "I" (_SFR_IO_ADDR(PORTE))
  );
}

void dataIn_WS()
{
  DDRC = 0x00;
  DDRA = 0x00;
}

void dataOut_WS()
{
  DDRC = 0xff;
  DDRA = 0xff;
}
