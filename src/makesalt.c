/*
 * makesalt.c -- handles:
 * making the salt for the encryption.
 *
 */

/************************************************************************
 *   psybnc2.2.2, tools/makesalt.c
 *   Copyright (C) 2001 the most psychoid  and
 *                      the cool lam3rz IRC Group, IRCnet
 *			http://www.psychoid.lam3rz.de
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

char rbuf[100];

const char *randstring(int length)
{
    char *po;
    int i;
    po=rbuf;
    if (length>100) length=100;
    for(i=0;i<length;i++) {*po=(char)(0x61+(rand()&15)); po++;}
    *po=0;
    po=rbuf;
    return po;
}

int main(void)
{

    FILE* salt;
    int saltlen1;
    int saltlen2;
    int foo;
    srand(time(NULL));
    saltlen1=(rand()&20)+5;
    saltlen2=(rand()&20)+5;
    if ( (salt=fopen("pack/salt.h","r"))!=NULL) {
	fclose(salt);
	printf("Using existent Salt-File\n");
	exit(0x0);
    }
    printf("Creating Salt File\n");
    if ( (salt=fopen("pack/salt.h","w"))==NULL) {
	printf("Cannot created Salt-File.. aborting\n");
	exit(0x1);
    }
    fprintf(salt,"/* The 1. Salt -> string containing anything, %d chars */\n",saltlen1);
    fprintf(salt,"#define SALT1 %c%s%c\n",34,randstring(saltlen1),34);
    fprintf(salt,"\n");
    fprintf(salt,"/* The 2. Salt -> string containing anything, %d chars */\n",saltlen2);
    fprintf(salt,"#define SALT2 %c%s%c\n",34,randstring(saltlen2),34);
    fprintf(salt,"\n");
    fprintf(salt,"/* the 1. Code -> a one byte startup code */\n");
    fprintf(salt,"#define CODE1 %d\n",64+(rand()&15));
    fprintf(salt,"\n");
    fprintf(salt,"/* the 2. Code -> a one byte startup code */\n");
    fprintf(salt,"#define CODE2 %d\n",64+(rand()&15));
    fprintf(salt,"\n");
    fprintf(salt,"/* the 1. Salt Offset -> value from 0-%d */\n",saltlen1-1);
    fprintf(salt,"#define SA1 %d\n",rand()&(saltlen1-1));
    fprintf(salt,"\n");
    fprintf(salt,"/* the 2. Salt Offset -> value from 0-%d */\n",saltlen2-1);
    fprintf(salt,"#define SA2 %d\n",rand()&(saltlen2-1));
    fprintf(salt,"\n");
    fprintf(salt,"/* the make salt routine */\n");
    fprintf(salt,"/* dont wonder about the redundance, its needed to somehow hide the fully salts */\n");
    fprintf(salt,"\n");
    fprintf(salt,"/* salt buffers */\n");
    fprintf(salt,"\n");
    fprintf(salt,"unsigned char slt1[%d];\n",saltlen1+1);
    fprintf(salt,"unsigned char slt2[%d];\n",saltlen2+1);
    fprintf(salt,"\n");
    fprintf(salt,"int makesalt(void)\n");
    fprintf(salt,"{\n");
    for (foo=0;foo<saltlen1;foo++) 
        fprintf(salt,"    slt1[%d]=SALT1[%d];\n",foo,foo);
    fprintf(salt,"    slt1[%d]=0;\n",saltlen1);
    for (foo=0;foo<saltlen2;foo++) 
        fprintf(salt,"    slt2[%d]=SALT2[%d];\n",foo,foo);
    fprintf(salt,"    slt2[%d]=0;\n",saltlen2);
    fprintf(salt,"return 0;\n");
    fprintf(salt,"}");
    fprintf(salt,"\n");
    fclose(salt);
    printf("Salt File created.\n");
    exit (0x0);
}
