

/*
   UCF 2015 (Fall) Local Programming Contest
   Problem: twins
*/

#include <stdio.h>

#define TRUE  1
#define FALSE 0

#define PLAYER_COUNT 10

#define MACK_NUMBER 18
#define ZACK_NUMBER 17

/* ************************************************************ */

int main(void)
{
   int   data_set_count, player_number, k, j;
   int   found_mack, found_zack;
   FILE  *in_fptr;

   in_fptr = stdin;

   fscanf(in_fptr, "%d", &data_set_count);
   for ( k = 1;  k <= data_set_count;  ++k )
   {
      found_mack = FALSE;
      found_zack = FALSE;

      for ( j = 1;  j <= PLAYER_COUNT;  ++j )
        {
         fscanf(in_fptr, "%d", &player_number);
         printf("%d", player_number);
         if ( j < PLAYER_COUNT )
            printf(" ");
         if ( player_number == MACK_NUMBER )
            found_mack = TRUE;
         else if ( player_number == ZACK_NUMBER )
            found_zack = TRUE;
        }

      if ( found_mack && found_zack )
         printf("\nboth\n\n");
      else if ( found_mack )
         printf("\nmack\n\n");
      else if ( found_zack )
         printf("\nzack\n\n");
      else
         printf("\nnone\n\n");

   }/* end for ( k ) */

   return(0);

}/* end main */

/* ************************************************************ */

