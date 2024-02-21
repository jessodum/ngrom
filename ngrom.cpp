// New GROM - Genesis ROM conversion (SMD->BIN) utility
// Based on the GROM 0.75 source code by Bart Trzynadlowski, 2000.

#include<QCoreApplication>
#include<QCommandLineParser>
#include<QFileInfo>
#include<stdio.h>  // for FILE I/O
#include<iostream> // for std::cout and std::err
#include<errno.h>
#include<string.h>

namespace NGROM_NS
{
   enum RomFormat
   {
      UNK_FMT,
      SMD,
      BIN
   };

   enum FileCheckAction
   {
      UNSET,
      STOP,
      WARN,
      SKIP
   };
}

// Constants
static const size_t NUM_HEADER_BYTES = 512;
static const size_t NUM_SMD_BLOCK_BYTES = 16384; // 16KB

// Function prototypes
NGROM_NS::FileCheckAction parseFileCheckActionString(const QString& fileCheckActionString);
bool checkFormats(NGROM_NS::RomFormat fmt, const QStringList& filenameList);
NGROM_NS::RomFormat getLikelyFormat(const unsigned char* headerBytes);
void decodeSMDBlock(unsigned char* binBlock, const unsigned char* smdBlock);
void showInfoList(const QStringList& filenameList);
bool convertFiles(const QStringList& filenameList,
                  const std::string& outdir,
                  NGROM_NS::FileCheckAction fileCollisionAction);


// -----------------------------------------------------------------------------
// Exit Codes:
//    0 = No error
//    1 = Error with command line argument(s)
//    2 = Stopped due to integrity check
// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  // Setup the application object
   QCoreApplication theApp(argc, argv);
   QCoreApplication::setApplicationName("ngrom");
   QCoreApplication::setApplicationVersion("0.1.0");

  // Setup the command line parser
   QCommandLineParser argsParser;
   argsParser.setApplicationDescription("New GROM - Genesis ROM conversion utility");
   argsParser.addHelpOption();
   argsParser.addVersionOption();

  // Specify command line arguments (strings supplied for auto-generated help text)
   QCommandLineOption infoOption(QStringList() << "i" << "info",
      "Show information about the file(s) instead of doing conversion(s).");
   argsParser.addOption(infoOption);

   QCommandLineOption doChecksOption(QStringList() << "c" << "checks",
      "Performing checks of the ROM formats. Options are \"stop\", \"warn\", or \"skip\". \"stop\" [default] will stop (exit) the program if any ROM format check fails, whereas \"warn\" will simply issue a warning and attempt to continue. \"skip\" will skip performing any checks at all.",
      "checkOpt",
      "stop");
   argsParser.addOption(doChecksOption);

   QCommandLineOption fileCollideOption(QStringList() << "f" << "file-collision",
      "Action to perform if an output file already exists. Options are same as <checkOpt>. \"stop\" will stop (exit) the program when an output file name is found to already exist. \"warn\" will issue a warning and (attempt to) overwrite the file. \"skip\" [default] will issue a warning and skip writing the output file.",
      "fileAction",
      "skip");
   argsParser.addOption(fileCollideOption);

   QCommandLineOption outdirOption(QStringList() << "o" << "outdir",
      "Specifies the output directory. Default is current working directory. This option is ignored if --info is specified.",
      "outdir");
   argsParser.addOption(outdirOption);

   argsParser.addPositionalArgument("files",
      "(SMD) Files to convert. Output file names will have the .bin extension (replacing the .smd extension, if it exists).",
      "[files...]");

  // Parse the command line arguments!
   argsParser.process(theApp);

  // Get list of (input) files specified...
   const QStringList argsList = argsParser.positionalArguments();

  // Exit if no files specified.
   if (argsList.isEmpty())
   {
      std::cerr << "NGROM ERROR: No files specified." << std::endl;
      return 1;
   }

  // Validate the file check options
   QString checkOptString = argsParser.value(doChecksOption);
   NGROM_NS::FileCheckAction checkOpt = parseFileCheckActionString(checkOptString);
   if (checkOpt == NGROM_NS::UNSET)
   {
      std::cerr << "NGROM ERROR: Unrecognized checkOpt: " << checkOptString.toStdString() << std::endl;
      argsParser.showHelp(1);
   }

   QString fileCollideActionString = argsParser.value(fileCollideOption);
   NGROM_NS::FileCheckAction fileAction = parseFileCheckActionString(fileCollideActionString);
   if (fileAction == NGROM_NS::UNSET)
   {
      std::cerr << "NGROM ERROR: Unrecognized fileAction: " << fileCollideActionString.toStdString() << std::endl;
      argsParser.showHelp(1);
   }

  // Do SMD format checks, if allowed.
   if (checkOpt == NGROM_NS::SKIP)
   {
      std::cout << "Skipping SMD format checks..." << std::endl;
   }
   else
   {
      bool rc = checkFormats(NGROM_NS::SMD, argsList);
      if (rc == false)
      {
         if (checkOpt == NGROM_NS::STOP)
         {
            std::cout << "NGROM stopping due to failed SMD format check on one or more files" << std::endl;
            return 2;
         }
         else if (checkOpt == NGROM_NS::WARN)
         {
            std::cerr << "NGROM WARNING: one or more files failed SMD format check; continuing..." << std::endl;
         }
      }
   }

  // Do the action
   if (argsParser.isSet(infoOption))
   {
      showInfoList(argsList);
   }
   else
   {
      // Set output directory
      std::string outdir = ".";

      if (argsParser.isSet("outdir"))
      {
         outdir = argsParser.value("outdir").toStdString();
      }

      // Do conversions!
      bool rc = convertFiles(argsList, outdir, fileAction);
      if (rc == false)
      {
         std::cout << "NGROM stopping due to error writing an output file" << std::endl;
         return 2;
      }
   }

  // Done!
   return 0;
}


// -----------------------------------------------------------------------------
// Function: parseFileCheckActionString
// Description: Converts an argument string into a NGROM_NS::FileCheckAction
//              enum value.
// Return: NGROM_NS::FileCheckAction value based on the supplied string.
//         UNSET if string is not recognized.
// -----------------------------------------------------------------------------
NGROM_NS::FileCheckAction parseFileCheckActionString(const QString& fileCheckActionString)
{
   NGROM_NS::FileCheckAction retval = NGROM_NS::UNSET;

   if (fileCheckActionString == "stop")
   {
      retval = NGROM_NS::STOP;
   }
   else if (fileCheckActionString == "warn")
   {
      retval = NGROM_NS::WARN;
   }
   else if (fileCheckActionString == "skip")
   {
      retval = NGROM_NS::SKIP;
   }
   // else, unrecognized string; UNSET is already the retval.

   return retval;
}

// -----------------------------------------------------------------------------
// Function: getLikelyFormat
// Description: Checks the supplied header bytes for ROM format markers and
//              returns the most likely format.
// Return: Most likely format, or UNK_FMT if indeterminate.
// -----------------------------------------------------------------------------
NGROM_NS::RomFormat getLikelyFormat(const unsigned char* headerBytes)
{
   NGROM_NS::RomFormat retval = NGROM_NS::UNK_FMT;

   // BIN files have "SEGA" starting at byte offset 0x100.
   if (0 == memcmp(headerBytes + 0x100, "SEGA", 4))
   {
      retval = NGROM_NS::BIN;
   }

   // SMD files should have 0xAA at byte offset 8, and 0xBB at byte offset 9.
   else if ((headerBytes[8] == 0xAA) && (headerBytes[9] == 0xBB))
   {
      retval = NGROM_NS::SMD;
   }

   return retval;
}

// -----------------------------------------------------------------------------
// Function: checkFormats
// Description: Checks each of the input files from the supplied list to ensure
//              the conform to the indicated ROM format.
// Return: true if all files pass the checks successfully;
//         false if any error occurred.
// -----------------------------------------------------------------------------
bool checkFormats(NGROM_NS::RomFormat fmt, const QStringList& filenameList)
{
   bool retval = true;

   unsigned char tmpBytes[NUM_HEADER_BYTES];
   memset(tmpBytes, 0, NUM_HEADER_BYTES);

   for (QString filename : filenameList)
   {
      if (fmt == NGROM_NS::BIN)
      {
         std::cout << "Checking file for BIN format: " << filename.toStdString() << std::endl;

         FILE* inFile = fopen(filename.toStdString().c_str(), "r");
         if (inFile == NULL)
         {
            int saved_errno = errno;
            std::cerr << "  NGROM ERROR: Failed to open file... " << strerror(saved_errno) << std::endl;
            retval = false;
         }
         else
         {
            // Clear bytes buffer
            memset(tmpBytes, 0, NUM_HEADER_BYTES);

            size_t numBytesRead = fread(tmpBytes, 1, NUM_HEADER_BYTES, inFile);
            if (numBytesRead < NUM_HEADER_BYTES)
            {
               std::cerr << "  NGROM ERROR: Incomplete read..." << std::endl;
               retval = false;
            }
            else
            {
               // BIN files have "SEGA" starting at byte offset 0x100.
               if (0 == memcmp(tmpBytes + 0x100, "SEGA", 4))
               {
                  std::cout << "  ...GOOD!" << std::endl;
               }
               else
               {
                  std::cout << "  ...FAILED!" << std::endl;
                  retval = false;
               }
            }
            fclose(inFile);
         }
      }
      else if (fmt == NGROM_NS::SMD)
      {
         std::cout << "Checking file for SMD format: " << filename.toStdString() << std::endl;

         FILE* inFile = fopen(filename.toStdString().c_str(), "r");
         if (inFile == NULL)
         {
            int saved_errno = errno;
            std::cerr << "  NGROM ERROR: Failed to open file... " << strerror(saved_errno) << std::endl;
            retval = false;
         }
         else
         {
            // Clear bytes buffer
            memset(tmpBytes, 0, NUM_HEADER_BYTES);

            size_t numBytesRead = fread(tmpBytes, 1, NUM_HEADER_BYTES, inFile);
            if (numBytesRead < NUM_HEADER_BYTES)
            {
               std::cerr << "  NGROM ERROR: Incomplete read..." << std::endl;
               retval = false;
            }
            else
            {
               // SMD files should have 0xAA at byte offset 8, and 0xBB at byte offset 9.
               // They should also not have the BIN "SEGA" text at byte offset 0x100.
               if ((tmpBytes[8] != 0xAA) || (tmpBytes[9] != 0xBB))
               {
                  std::cout << "  ...FAILED!" << std::endl;
                  retval = false;
               }
               else
               {
                  // GOOD so far; check for "SEGA"
                  if (0 == memcmp(tmpBytes + 0x100, "SEGA", 4))
                  {
                     std::cout << "  ...FAILED! (appears to be BIN format)" << std::endl;
                     retval = false;
                  }
                  else
                  {
                     std::cout << "  ...GOOD!" << std::endl;
                  }
               }
            }
            fclose(inFile);
         }
      }
      else
      {
         std::cerr << "NGROM ERROR: checkFormats not implemented for specified fmt: " << fmt << std::endl;
         return false;
      }
   }

   return retval;
}

// -----------------------------------------------------------------------------
// Function: decodeSMDBlock
// Description: Converts a 16KB SMD block to a BIN block.
// -----------------------------------------------------------------------------
void decodeSMDBlock(unsigned char* destBINBlock, const unsigned char* srcSMDBlock)
{
   size_t evenByte = 0;
   size_t oddByte = 1;

   for (int i = 0; i < 8192; oddByte += 2, evenByte += 2, i++)
   {
      destBINBlock[oddByte]  = srcSMDBlock[i];
      destBINBlock[evenByte] = srcSMDBlock[i+8192];
   }
}

// -----------------------------------------------------------------------------
// Function: showInfoList
// Description: Parses metadata embedded in each of the input files from the
//              supplied list and displays them to STDOUT.
// -----------------------------------------------------------------------------
void showInfoList(const QStringList& filenameList)
{
   // Header is only 512 bytes, but the SMD format contains the desired info
   // within a 16 KB SMD block.  The function to decode the block will need the
   // full allocation in the destination buffer.
   unsigned char tmpHeaderBytes[NUM_SMD_BLOCK_BYTES];
   memset(tmpHeaderBytes, 0, NUM_SMD_BLOCK_BYTES);

   unsigned char tmpSMDBlock[NUM_SMD_BLOCK_BYTES];
   memset(tmpSMDBlock, 0, NUM_SMD_BLOCK_BYTES);

   for (QString filename : filenameList)
   {
      std::cout << "Showing info from ROM data for file: " << filename.toStdString() << std::endl;

      FILE* inFile = fopen(filename.toStdString().c_str(), "r");
      if (inFile == NULL)
      {
         int saved_errno = errno;
         std::cerr << "  NGROM ERROR: Failed to open file... " << strerror(saved_errno) << std::endl;
         std::cout << "  ... skipping." << std::endl;
      }
      else
      {
         // Clear bytes buffer
         memset(tmpHeaderBytes, 0, NUM_SMD_BLOCK_BYTES);

         size_t numBytesRead = fread(tmpHeaderBytes, 1, NUM_HEADER_BYTES, inFile);
         if (numBytesRead < NUM_HEADER_BYTES)
         {
            std::cerr << "  NGROM ERROR: Incomplete read..." << std::endl;
            std::cout << "  ... skipping." << std::endl;
         }
         else
         {
            bool okToContinue = true;
            NGROM_NS::RomFormat likelyFmt = getLikelyFormat(tmpHeaderBytes);

            if (likelyFmt == NGROM_NS::UNK_FMT)
            {
               std::cerr << "  NGROM ERROR: Unrecognized file format..." << std::endl;
               std::cout << "  ... skipping." << std::endl;
               okToContinue = false;
            }
            else if (likelyFmt == NGROM_NS::SMD)
            {
               // Get first SMD block and decode it.
               // NOTE: at this point we've already skipped the first 512 bytes of the file.
               numBytesRead = fread(tmpSMDBlock, 1, NUM_SMD_BLOCK_BYTES, inFile);
               if (numBytesRead < NUM_HEADER_BYTES)
               {
                  std::cerr << "  NGROM ERROR: Incomplete read..." << std::endl;
                  std::cout << "  ... skipping." << std::endl;
                  okToContinue = false;
               }
               else
               {
                  // Clear destination buffer (again)
                  memset(tmpHeaderBytes, 0, NUM_SMD_BLOCK_BYTES);

                  // Decode the SMD block containing the header info
                  decodeSMDBlock(tmpHeaderBytes, tmpSMDBlock);
               }
            }

            if (okToContinue)
            {
               char decodedChars[50];  // It looks like from GROM, the largest string is
                                       // only 48 characters, but I like nice round numbers.
               char hexChars[10];  // Gonna use snprintf to format bytes into hex characters.
                                   // (Again rounding up to a nice multiple of 10).

              // System
               memset(decodedChars, 0, 50);
               memcpy(decodedChars, &tmpHeaderBytes[0x100], 16);
               std::cout << "                    System: " << decodedChars << std::endl;

              // Copyright
               memset(decodedChars, 0, 50);
               memcpy(decodedChars, &tmpHeaderBytes[0x110], 16);
               std::cout << "                 Copyrigth: " << decodedChars << std::endl;

              // Game name (domestic)
               memset(decodedChars, 0, 50);
               memcpy(decodedChars, &tmpHeaderBytes[0x120], 48);
               std::cout << "      Game name (domestic): " << decodedChars << std::endl;

              // Game name (overseas)
               memset(decodedChars, 0, 50);
               memcpy(decodedChars, &tmpHeaderBytes[0x150], 48);
               std::cout << "      Game name (overseas): " << decodedChars << std::endl;

              // Software type
               std::cout << "             Software type: ";
               if (tmpHeaderBytes[0x180] == 'G' && tmpHeaderBytes[0x181] == 'M')
               {
                  std::cout << "Game" << std::endl;
               }
               else if (tmpHeaderBytes[0x180] == 'A' && tmpHeaderBytes[0x181] == 'l')
               {
                  std::cout << "Educational" << std::endl;
               }
               else
               {
                  std::cout << (char)tmpHeaderBytes[0x180] << (char)tmpHeaderBytes[0x181] << std::endl;
               }

              // Comment from Bart's original GROM source code:
              //  ""From personal observation, it seems the product code field starts at 0x183, and
              //    is 11 bytes long.  0x182 may be a continuation of the software type field, but I
              //    am most likely wrong.""

              // Product code and version
               memset(decodedChars, 0, 50);
               memcpy(decodedChars, &tmpHeaderBytes[0x183], 11);
               std::cout << "  Product code and version: " << decodedChars << std::endl;

              // Checksum
               memset(hexChars, 0, 10);
               snprintf(hexChars, 5, "%02X%02X", tmpHeaderBytes[0x18e], tmpHeaderBytes[0x18f]);
               std::cout << "                  Checksum: 0x" << hexChars << std::endl;

              // I/O support
               memset(decodedChars, 0, 50);
               memcpy(decodedChars, &tmpHeaderBytes[0x190], 16);
               std::cout << "               I/O support: " << decodedChars << std::endl;

              // Comment from Bart's original GROM source code:
              //  ""The meaning of these fields may have been misinterpreted.""

              // ROM start address
               memset(hexChars, 0, 10);
               snprintf(hexChars, 9, "%02X%02X%02X%02X", tmpHeaderBytes[0x1a0], tmpHeaderBytes[0x1a1],
                                                         tmpHeaderBytes[0x1a2], tmpHeaderBytes[0x1a3]);
               std::cout << "         ROM start address: 0x" << hexChars << std::endl;

              // ROM end address
               memset(hexChars, 0, 10);
               snprintf(hexChars, 9, "%02X%02X%02X%02X", tmpHeaderBytes[0x1a4], tmpHeaderBytes[0x1a5],
                                                         tmpHeaderBytes[0x1a6], tmpHeaderBytes[0x1a7]);
               std::cout << "           ROM end address: 0x" << hexChars << std::endl;

              // Comment from Bart's original GROM source code:
              //  ""Is the modem data field really 20 bytes?
              //    XnaK's document seems to indicate it is only 10...""

              // Modem data
               memset(decodedChars, 0, 50);
               memcpy(decodedChars, &tmpHeaderBytes[0x1bc], 20);
               std::cout << "                Modem data: " << decodedChars << std::endl;

              // Memo
               memset(decodedChars, 0, 50);
               memcpy(decodedChars, &tmpHeaderBytes[0x1c8], 40);
               std::cout << "                      Memo: " << decodedChars << std::endl;

              // Countries
               memset(decodedChars, 0, 50);
               memcpy(decodedChars, &tmpHeaderBytes[0x1f0], 3);
               std::cout << "                 Countries: " << decodedChars << std::endl;
            }
         }
         fclose(inFile);
      }
   }
}

// -----------------------------------------------------------------------------
// Function: convertFiles
// Description: Performs the (SMD->BIN) ROM format conversion on each of the
//              input files from the supplied list.
// Return: true if output files written successfully;
//         false if any error occurred.
// -----------------------------------------------------------------------------
bool convertFiles(const QStringList& filenameList,
                  const std::string& outdir,
                  NGROM_NS::FileCheckAction fileCollisionAction)
{
   bool retval = true;
   unsigned char smdBlockBytes[NUM_SMD_BLOCK_BYTES];
   unsigned char binBlockBytes[NUM_SMD_BLOCK_BYTES];

   for (QString filename : filenameList)
   {
      // Determine output file path/name
      QFileInfo inFileInfo(filename);

      QString outFilename = inFileInfo.fileName();

      if (inFileInfo.suffix().toLower() == "smd")
      {
         // Replace extension with "bin"
         size_t fnameLen = outFilename.length();
         outFilename.replace(fnameLen-3, 3, "bin");
      }
      else
      {
         // Append extension ".bin"
         outFilename += ".bin";
      }

      std::string outFileFullPath = outdir;
      outFileFullPath += "/";
      outFileFullPath += outFilename.toStdString();

      std::cout << "Converting " << filename.toStdString() << std::endl
                << "        to " << outFileFullPath << std::endl;

      // Check for existing output file
      QFileInfo outFileInfo(outFileFullPath.c_str());

      if (outFileInfo.exists())
      {
         std::cerr << "  NGROM WARNING: Output file already exists!" << std::endl;
         if (fileCollisionAction == NGROM_NS::STOP)
         {
            // STOP; must return now.
            return false;
         }
         else if (fileCollisionAction == NGROM_NS::SKIP)
         {
            // SKIP; move on to next input file.
            std::cout << "  ...skipping!" << std::endl;
            continue;
         }
         // else - WARN (attempt to overwrite the file).
      }

      // Determine number of "blocks" in the SMD file
      size_t fileSize = inFileInfo.size();

      if (fileSize < (NUM_HEADER_BYTES + NUM_SMD_BLOCK_BYTES))
      {
         std::cerr << "  NGROM ERROR: Input file is too small (only " << fileSize << " bytes)" << std::endl;
         return false;
      }

      size_t numBlocks = (fileSize - NUM_HEADER_BYTES) / NUM_SMD_BLOCK_BYTES;
      size_t extraBytes = (fileSize - NUM_HEADER_BYTES) % NUM_SMD_BLOCK_BYTES;
      if (extraBytes > 0)
      {
         std::cerr << "  NGROM ERROR: Input file does not end on 16KB block boundary (possible data corruption)." << std::endl;
         return false;
      }

      // Open input file
      FILE* inSMDFile = fopen(filename.toStdString().c_str(), "r");
      if (inSMDFile == NULL)
      {
         int saved_errno = errno;
         std::cerr << "  NGROM ERROR: Failed to open INPUT file... " << strerror(saved_errno) << std::endl;

         // Must return immediately
         return false;
      }

      // Open output file
      FILE* outBINFile = fopen(outFileFullPath.c_str(), "w");
      if (outBINFile == NULL)
      {
         int saved_errno = errno;
         std::cerr << "  NGROM ERROR: Failed to open OUTPUT file... " << strerror(saved_errno) << std::endl;

         // Must return immediately
         fclose(inSMDFile);
         return false;
      }

      // Skip header in SMD file
      fseek(inSMDFile, NUM_HEADER_BYTES, SEEK_SET);

      // Convert each of the blocks.
      bool okToContinue = true;
      for (size_t i = 0; i < numBlocks; i++)
      {
         // Reset data buffers
         memset(smdBlockBytes, 0, NUM_SMD_BLOCK_BYTES);
         memset(binBlockBytes, 0, NUM_SMD_BLOCK_BYTES);

         // Read in SMD block
         size_t numBytesRead = fread(smdBlockBytes, 1, NUM_SMD_BLOCK_BYTES, inSMDFile);
         if (numBytesRead < NUM_SMD_BLOCK_BYTES)
         {
            std::cerr << "  NGROM ERROR: Incomplete read of SMD block!" << std::endl;
            okToContinue = false;
            break;
         }

         // Convert to BIN block
         decodeSMDBlock(binBlockBytes, smdBlockBytes);

         // Write out BIN block
         size_t numBytesWritten = fwrite(binBlockBytes, 1, NUM_SMD_BLOCK_BYTES, outBINFile);
         if (numBytesWritten < NUM_SMD_BLOCK_BYTES)
         {
            std::cerr << "  NGROM ERROR: Incomplete write of BIN block!" << std::endl;
            okToContinue = false;
            break;
         }
      }

      fclose(inSMDFile);
      fclose(outBINFile);

      if (okToContinue)
      {
         std::cout << "  Conversion complete!" << std::endl;
      }
      else
      {
         return false;
      }
   }

   return retval;
}

