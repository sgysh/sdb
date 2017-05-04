/****************************************************************************
 * sample.c
 *
 *   Copyright (c) 2017 Yoshinori Sugino
 *   This software is released under the MIT License.
 ****************************************************************************/
#include <stdio.h>
#include <unistd.h>

void func(int i) {
  printf("count: %d\n", i);
}

int main(int argc, char *argv[]) {
  for (int i = 0; i < 5; ++i) {
    sleep(1);
    func(i);
  }

  return 0;
}

