<CsoundSynthesizer>

; Id: D06_CUT04.CSD mg (2006, rev.2009)
; author: marco gasperini (marcogsp at yahoo dot it)

; G.M. Koenig
; ESSAY (1957)

<CsOptions>
-W -f -oD06_CUT04.wav
</CsOptions>

<CsInstruments>

sr     = 192000
kr     = 19200
ksmps  = 10
nchnls = 1

;===========================================================
; 226.4 OTHER TRANSFORMATIONS (TAPE CUTS)
;===========================================================
	instr 1
iskip	= p4/76.2
icut	= p3
	print iskip
inum	= p5-1
a1	diskin2 "D05_TR04.wav" , 1 , iskip + (icut*inum)
aenv	expseg .001 , .02, 1 , p3-.04, 1 , .02, .001

aout	= a1*aenv
	out aout
	endin
;===========================================================

</CsInstruments>
<CsScore>
t0 4572
;test---------------------------------------------------

;\test---------------------------------------------------

;================================================ 
; 246.41 (790 cm)
;================================================ 

;---------------------------------------- 8*19cm
i1	0	19	0	1
i1	+ 	. 	.	2
i1	+	. 	.	3
i1	+	. 	.	4
i1	+	. 	.	5
i1	+	. 	.	6
i1	+	. 	.	7
i1	+	. 	.	8
s
t0 4572
;---------------------------------------- 8*15.2cm
i1	0	15.2	152  	2
i1	+	. 	.  	8
i1	+	. 	.  	7
i1	+	. 	.  	5
i1	+	. 	.  	1
i1	+	. 	.  	3
i1	+	. 	.  	4
i1	+	. 	.  	6
s
t0 4572
;---------------------------------------- 8* 7.8cm
i1	0	7.8	273.6	3
i1	+	.	.	7
i1	+	.	.	1
i1	+	.	.	8
i1	+	.	.	6
i1	+	.	.	5
i1	+	.	.	2
i1	+	.	.	4
s
t0 4572
;---------------------------------------- 8* 4.95cm
i1	0	4.95	336	4
i1	+	.	.	5
i1	+	.	.	8
i1	+	.	.	3
i1	+	.	.	7
i1	+	.	.	2
i1	+	.	.	6
i1	+	.	.	1
s
t0 4572
;---------------------------------------- 8* 23.75cm
i1	0	23.75	375.6	5
i1	+	.	.	1
i1	+	.	.	6
i1	+	.	.	7
i1	+	.	.	4
i1	+	.	.	8
i1	+	.	.	3
i1	+	.	.	2
s
t0 4572
;---------------------------------------- 8* 9.7cm
i1	0	9.7	565.6	6
i1	+	.	.	3
i1	+	.	.	5
i1	+	.	.	2
i1	+	.	.	8
i1	+	.	.	4
i1	+	.	.	1
i1	+	.	.	7
s
t0 4572
;---------------------------------------- 8* 6.2cm
i1	0	6.2	643.2	7
i1	+	.	.	4
i1	+	.	.	2
i1	+	.	.	6
i1	+	.	.	3
i1	+	.	.	1
i1	+	.	.	8
i1	+	.	.	5
s
t0 4572
;---------------------------------------- 8* 12.15cm
i1	0	12.15	692.8	5
i1	+	.	.	1
i1	+	.	.	6
i1	+	.	.	7
i1	+	.	.	4
i1	+	.	.	8
i1	+	.	.	3
i1	+	.	.	2
;================================================ \ 246.41 (790 cm)
s
t0 4572
;================================================   
;================================================   
; 246.42 (156 cm)
;================================================   
;================================================   

i1	0	156	790	1
;================================================ \ 246.42 (156 cm)
s

t0 4572
;================================================   
;================================================   
; 246.43 (1185.2 cm)
;================================================   
;================================================   

;---------------------------------------- 8* 9.75cm
i1	0	9.75	946	1
i1	+	.	.	2
i1	+	.	.	3
i1	+	.	.	4
i1	+	.	.	5
i1	+	.	.	6
i1	+	.	.	7
i1	+	.	.	8
s
t0 4572
;---------------------------------------- 8* 7.35cm
i1	0	7.35	1024	2
i1	+	.	.	8
i1	+	.	.	7
i1	+	.	.	5
i1	+	.	.	1
i1	+	.	.	3
i1	+	.	.	4
i1	+	.	.	6
s
t0 4572
;---------------------------------------- 8* 30.85cm
i1	0	30.85	1082.2	3
i1	+	.	.	7
i1	+	.	.	1
i1	+	.	.	8
i1	+	.	.	6
i1	+	.	.	5
i1	+	.	.	2
i1	+	.	.	4
s
t0 4572
;---------------------------------------- 8* 17.35cm
i1	0	17.35	1329.6	4
i1	+	.	.	5
i1	+	.	.	8
i1	+	.	.	3
i1	+	.	.	7
i1	+	.	.	2
i1	+	.	.	6
i1	+	.	.	1
s
t0 4572
;---------------------------------------- 8* 13.05cm
i1	0	13.05	1468.4	5
i1	+	.	.	1
i1	+	.	.	6
i1	+	.	.	7
i1	+	.	.	4
i1	+	.	.	8
i1	+	.	.	3
i1	+	.	.	2
s
t0 4572
;---------------------------------------- 8* 41.15cm
i1	0	41.15	1572.8	6
i1	+	.	.	3
i1	+	.	.	5
i1	+	.	.	2
i1	+	.	.	8
i1	+	.	.	4
i1	+	.	.	1 
i1	+	.	.	7
s
t0 4572
;---------------------------------------- 8* 23.15cm
i1	0	23.15	1902	7
i1	+	.	.	4
i1	+	.	.	2
i1	+	.	.	6
i1	+	.	.	3
i1	+	.	.	1
i1	+	.	.	8
i1	+	.	.	5
s
t0 4572
;---------------------------------------- 8* 5.5cm
i1	0	5.5	2087.2	8
i1	+	.	.	6
i1	+	.	.	4
i1	+	.	.	1
i1	+	.	.	2
i1	+	.	.	7
i1	+	.	.	5
i1	+	.	.	3
;================================================ \ 246.43 (1185.2 cm)
s

t0 4572
;================================================   
; 246.44 (349.6 cm)
;================================================   

i1	0	349.6	2131.2	1
;================================================ \ 246.44 (349.6 cm)
s

t0 4572
;================================================   
; 246.45 (426.8 cm)
;================================================   

;---------------------------------------- 8* 4.8cm
i1	0	4.8	2480.8	1
i1	+	.	.	2
i1	+	.	.	3
i1	+	.	.	4
i1	+	.	.	5
i1	+	.	.	6
i1	+	.	.	7
i1	+	.	.	8
s
t0 4572
;---------------------------------------- 8* 4cm
i1	0	4	2519.2	2
i1	+	.	.	8
i1	+	.	.	7
i1	+	.	.	5
i1	+	.	.	1
i1	+	.	.	3
i1	+	.	.	4
i1	+	.	.	6
s
t0 4572
;---------------------------------------- 8* 9.95cm
i1	0	9.95	2551.2	3
i1	+	.	.	7
i1	+	.	.	1
i1	+	.	.	8
i1	+	.	.	6
i1	+	.	.	5
i1	+	.	.	2
i1	+	.	.	4
s
t0 4572
;---------------------------------------- 8* 6.9cm
i1	0	6.9	2630.8	4
i1	+	.	.	5
i1	+	.	.	8
i1	+	.	.	3
i1	+	.	.	7
i1	+	.	.	2
i1	+	.	.	6
i1	+	.	.	1
s
t0 4572
;---------------------------------------- 8* 5.75cm
i1	0	5.75	2686	5
i1	+	.	.	1
i1	+	.	.	6
i1	+	.	.	7
i1	+	.	.	4
i1	+	.	.	8
i1	+	.	.	3
i1	+	.	.	2
s
t0 4572
;---------------------------------------- 8* 11.9cm
i1	0	11.9	2732	6
i1	+	.	.	3
i1	+	.	.	5
i1	+	.	.	2
i1	+	.	.	8
i1	+	.	.	4
i1	+	.	.	1 
i1	+	.	.	7
s
t0 4572
;---------------------------------------- 8* 8.25cm
i1	0	8.25	2827.2	7
i1	+	.	.	4
i1	+	.	.	2
i1	+	.	.	6
i1	+	.	.	3
i1	+	.	.	1
i1	+	.	.	8
i1	+	.	.	5
s
t0 4572
;---------------------------------------- 8* 14.3cm
i1	0	14.3	2893.2	8
i1	+	.	.	6
i1	+	.	.	4
i1	+	.	.	1
i1	+	.	.	2
i1	+	.	.	7
i1	+	.	.	5
i1	+	.	.	3
s

t0 4572
;================================================   
; 246.46 (454.8 cm)
;================================================   

i1	0	454.8	3007.6	1
;================================================ \ 246.46 (454.8 cm)
e
</CsScore>
</CsoundSynthesizer>