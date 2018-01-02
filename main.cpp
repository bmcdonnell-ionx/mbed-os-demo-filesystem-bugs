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
//#include "SDBlockDevice.h"
#include "HeapBlockDevice.h"

// File systems
//#include "LittleFileSystem.h"
#include "FATFileSystem.h"

#define FAT32_MAX_FILES_PER_DIR (65534)
#define NUM_FILES_PER_LOOP      (  250)
#define SERIAL_BAUD_RATE (115200)


// Trial and error suggests each empty file is consuming 32 KiB on the
//  block device.
// At this rate, 65534 files take > 2 MiB, so make a 3 MiB block device.
// Physical block device, can be any device that supports the BlockDevice API
HeapBlockDevice bd(3*1024*1024); // 3 MiB

// File system declaration
FATFileSystem fs("fs", &bd);


static void printDirListing(DIR *d) {
    while (true) {
         struct dirent *e = readdir(d);
         if (!e) {
              break;
         }

         printf("    %s\r\n", e->d_name);
    }
}


static void printRootAndTestDirListing() {
    // Display the root directory
    printf("Opening the root directory... ");
    fflush(stdout);
    DIR *d = opendir("/fs/");
    printf("%s\r\n", (!d ? "Fail :(" : "OK"));
    if (!d) {
         error("error: %s (%d)\r\n", strerror(errno), -errno);
    }

    printf("root directory:\r\n");
    printDirListing(d);

    // Display the test directory
    printf("Opening the test directory... ");
    fflush(stdout);
    d = opendir("/fs/fs-test");
    printf("%s\r\n", (!d ? "Fail :(" : "OK"));
    if (!d) {
         error("error: %s (%d)\r\n", strerror(errno), -errno);
    }

    printf("test directory:\r\n");
    printDirListing(d);

    printf("Closing the root directory... ");
    fflush(stdout);
    int err = closedir(d);
    printf("%s\r\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
         error("error: %s (%d)\r\n", strerror(errno), -errno);
    }
}


static void createEmptyTestFile(size_t num) {
    char path[30];
    sprintf(path, "/fs/fs-test/%08x.bin", num);
    fflush(stdout);
    FILE *pfout = fopen(path, "w");
    if (pfout != NULL)
    {
        int ret = fclose(pfout);

        if (ret != 0)
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
}


// Entry point for the example
int main() {
    sdram_init();

    printf("\r\n--- Mbed OS filesystem example ---\r\n");
    printf("Bug(?) demo 02: File write time increases\r\n");
    printf("...\r\n" // TODO: elaborate...
          "Use a big HeapBlockDevice (not SD Card) for speed.\r\n\r\n");
    fflush(stdout);

    // Setup the irq in case we want to use it
    //irq.fall(erase);

    // Try to mount the filesystem
    printf("Mounting the filesystem... ");
    fflush(stdout);
    int err = fs.mount(&bd);
    printf("%s\r\n", (err ? "Fail :(" : "OK"));
    if (err) {
        // Reformat if we can't mount the filesystem
        // this should only happen on the first boot
        printf("No filesystem found, formatting... ");
        fflush(stdout);
        err = fs.reformat(&bd);
        printf("%s\r\n", (err ? "Fail :(" : "OK"));
        if (err) {
            error("error: %s (%d)\r\n", strerror(-err), err);
        }
    }

    char const testDirPath[] = "/fs/fs-test";
    printf("Create test directory %s.\r\n", testDirPath);
    mkdir(testDirPath, S_IRWXU | S_IRWXG | S_IRWXO);

    Timer timer;

    printf("Creating files...\r\n");
    for (size_t i = 0; i < (FAT32_MAX_FILES_PER_DIR - 1); i += NUM_FILES_PER_LOOP)
    {
        printf("  Files %5u - %5u: ", i, (i + NUM_FILES_PER_LOOP - 1) );
        fflush(stdout);

        timer.reset();
        timer.start();
        for (size_t j = i; j < (i + NUM_FILES_PER_LOOP); j++)
        {
            createEmptyTestFile(j);
        }
        timer.stop();

        printf("%6i ms\r\n", timer.read_ms());
    }
    printf("\r\nDone.\r\n");

    printf("\r\n\r\n"
           "**********\r\n");
    printRootAndTestDirListing();

    printf("\r\n\r\n**********\r\n");

    // Tidy up
    printf("Unmounting... ");
    fflush(stdout);
    err = fs.unmount();
    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(-err), err);
    }
        
    printf("Mbed OS filesystem example done!\r\n");
    fflush(stdout);

    while(true)
    {
       wait_ms(500);
    }
}
