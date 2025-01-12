# CLOCKGUSTO

This project is about adapting the original QLOCKTWO to a new format. The QLOCKTWO is a unique clock that displays time in words rather than numbers, with illuminated letters forming phrases such as "IT IS HALF PAST TEN."

## Features

- **Minute Precision:** Each illuminated dot represents one minute past the hour.
- **Word-Based Time Display:** The clock displays time using words for a more intuitive reading experience.
- **REST API:** Allows for remote adjustment of the time settings via a RESTful API.
- **Android App Integration:** An Android app is available to adjust the time and retrieve log information.

## Time Display Examples
In the default setting, the introductory words "ES IST" (IT IS) are shown at full and half hours. Here are some examples of how the time is displayed:

- 00:00: ES IST ZWÖLF UHR (It is twelve o'clock)
- 00:05: FÜNF NACH ZWÖLF (Five past twelve)
- 00:10: ZEHN NACH ZWÖLF (Ten past twelve)
- 00:15: VIERTEL EINS (Quarter past twelve)
- 00:20: ZWANZIG NACH ZWÖLF (Twenty past twelve)
- 00:25: FÜNF VOR HALB EINS (Five to half past twelve)
- 00:30: ES IST HALB EINS (It is half past twelve)
- 00:35: FÜNF NACH HALB EINS (Five past half past twelve)
- 00:40: ZWANZIG VOR EINS (Twenty to one)
- 00:45: DREIVIERTEL EINS (Three-quarters to one)
- 00:50: ZEHN VOR EINS (Ten to one)
- 00:55: FÜNF VOR EINS (Five to one)
- 01:00: ES IST EIN UHR (It is one o'clock)

## Character Map Layout
```
.                       .
  
  E S K I S T A F Ü N F 		   
  Z E H N Z W A N Z I G 
  D R E I V I E R T E L 
  V O R F U N K N A C H 
  H A L B A E L F Ü N F
  E I N S X A M Z W E I
  D R E I P M J V I E R
  S E C H S N L A C H T
  S I E B E N Z W Ö L F
  Z E H N E U N K U H R

.                       .
```
## ESP LED Indices Plan
```
113 112
  .   .

 0  1  2  3  4  5  6  7  8  9 10
 E  S  K  I  S  T  A  F  Ü  N  F 	

21 20 19 18 17 16 15 14 13 12 11	   
 Z  E  H  N  Z  W  A  N  Z  I  G 
  
22 23 24 25 26 27 28 29 30 31 32
 D  R  E  I  V  I  E  R  T  E  L

43 42 41 40 39 38 37 36 35 34 33 
 V  O  R  F  U  N  K  N  A  C  H 

44 45 46 47 48 49 50 51 52 53 54
 H  A  L  B  A  E  L  F  Ü  N  F

65 64 63 62 61 60 59 58 57 56 55
 E  I  N  S  X  A  M  Z  W  E  I

66 67 68 69 70 71 72 73 74 75 76
 D  R  E  I  P  M  J  V  I  E  R

87 86 85 84 83 82 81 80 79 78 77
 S  E  C  H  S  N  L  A  C  H  T

88 89 90 91 92 93 94 95 96 97 98
 S  I  E  B  E  N  Z  W  Ö  L  F

109 108 107 106 105 104 103 102 101 100 99
 Z   E   H   N   E   U   N   K   U   H   R

 110 111
  .   .
```