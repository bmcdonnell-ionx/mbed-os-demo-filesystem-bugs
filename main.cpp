/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "mbed.h"
#include "sdram.h"
#include <stdio.h>
#include <errno.h>

// Block devices
//#include "SPIFBlockDevice.h"
//#include "DataFlashBlockDevice.h"
#include "SDBlockDevice.h"
//#include "HeapBlockDevice.h"

// File systems
//#include "LittleFileSystem.h"
#include "FATFileSystem.h"

#define MAX_FILES_PER_DIR (65534)
#define TEST_FILE_SIZE (512*1024)


// Physical block device, can be any device that supports the BlockDevice API
SDBlockDevice bd  (p5, p6, p7, p8); // mosi, miso, sclk, cs

// File system declaration
//FATFileSystem fs("fs", &bd);
FATFileSystem fs("fs");


static void *Zeroes = NULL;

static bool initTest()
{
   bool initOK;

   Zeroes = malloc(TEST_FILE_SIZE);
   initOK = (Zeroes != NULL);

   if (initOK)
   {
      memset(Zeroes, 0, TEST_FILE_SIZE);
   }

   return initOK;
}


static bool createTestFile(char const *path)
{
    FILE *pfout = fopen(path, "w");
    bool openOK  = (pfout != NULL);
    bool writeOK = false;
    bool closeOK = false;

    if (openOK)
    {
        writeOK = true;

        {
            int ret = fwrite(Zeroes, 1, TEST_FILE_SIZE, pfout);
            writeOK = (ret == TEST_FILE_SIZE);

            if (!writeOK)
            {
               printf("\r\n ERROR WRITING %s.\r\n", path);
               fflush(stdout);
            }
        }

        int ret = fclose(pfout);
        closeOK = (ret == 0);

        if (!closeOK)
        {
            printf("\r\n ERROR CLOSING %s.\r\n", path);
            fflush(stdout);
        }
    }
    else
    {
        printf("\r\nFAILED to create %s.\r\n", path);
        fflush(stdout);
    }
    fflush(stdout);

    return (openOK && writeOK && closeOK);
}


static void runtest()
{
   char const testDirPath[] = "/fs/fs-test";
   printf("Create test parent directory %s.\r\n", testDirPath);
   mkdir(testDirPath, S_IRWXU | S_IRWXG | S_IRWXO);

   char testSubdirPath[30];
   sprintf(testSubdirPath, "%s/00000000", testDirPath);
   printf("Create test directory %s.\r\n", testSubdirPath);
   mkdir(testSubdirPath, S_IRWXU | S_IRWXG | S_IRWXO);


   Timer timer;

   printf("Create files...\r\n");
   for (size_t i = 0x1fc0; i < MAX_FILES_PER_DIR; i++)
   {
       char filename[15];
       sprintf(filename, "%08x.bin", i);
       char path[45];
       sprintf(path, "%s/%s", testDirPath, filename);
       printf("%s ", path);
       fflush(stdout);

       timer.reset();
       timer.start();
       bool fileOK = createTestFile(path);
       timer.stop();

       printf("%6i ms\r\n", timer.read_ms());

       if (!fileOK) // quit on first failure
       {
          // createTestFile() prints error text
          break;
       }
   }
   printf("\r\nDone.\r\n");
}


static void recursiveDirectoryListing(char const *path)
{
   static unsigned int level = 0;

   DIR *d = opendir(path);
   if (!d)
   {
        error("error: %s (%d)\r\n", strerror(errno), -errno);
   }
   else
   {
      bool first = true;

      while (true)
      {
         struct dirent *e = readdir(d);
         if (!e)
         {
            if (first)
            {
               printf("  %*s(empty)\r\n", 2*level, "");
            }
            break;
         }

         first = false;
         bool isDir = (e->d_type == DT_DIR);

         // print each filename
         // append a trailing slash if it's a directory
         printf("  %*s%s%s\r\n", 2*level, "", e->d_name,
              (isDir ? "/" : ""));

         if (isDir)
         {
            char fullpath[sizeof ((struct dirent *)NULL)->d_name];
            strcpy(fullpath, path);
            if (fullpath[strlen(fullpath)-1] != '/')
            {
               strcat(fullpath, "/");
            }
            strcat(fullpath, e->d_name);
            level++;
            recursiveDirectoryListing(fullpath);
            level--;
         }
      }
   }
}


// Entry point for the example
int main() {
    sdram_init();

    printf("\r\n--- Mbed OS filesystem example ---\r\n"
           "Bug demo 03: Corrupt the filesystem on the SD Card.\r\n"
           "Requires a large block device (8+ GB).\r\n\r\n");
    fflush(stdout);

    // Setup the irq in case we want to use it
    //irq.fall(erase);

    // Try to mount the filesystem
    printf("Mounting the filesystem... ");
    fflush(stdout);
    int err = fs.mount(&bd);
    printf("%s\r\n", (err ? "Fail :(" : "OK"));
    if (err)
    {
        printf("No filesystem found.\r\n");
    }
    else
    {
       bool initOK = initTest();

       if (initOK)
       {
          printf("Disk contents before test:\r\n\r\n");
          recursiveDirectoryListing("/fs");

          runtest();

          printf("Disk contents after test:\r\n\r\n");
          recursiveDirectoryListing("/fs");
       }
       else
       {
          printf("Test initialization failure - abort.\r\n");
       }

       // Tidy up
       printf("Unmounting... ");
       fflush(stdout);
       int err = fs.unmount();
       printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
       if (err < 0) {
           error("error: %s (%d)\n", strerror(-err), err);
       }
    }

    printf("Mbed OS filesystem example done!\r\n");
    fflush(stdout);

    while(true)
    {
       wait_ms(500);
    }
}
