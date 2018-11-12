       IDENTIFICATION DIVISION.
       PROGRAM-ID. GUESTBK.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  TA PIC X(4). 
       01  GBKENTRY.
           05 TIT PIC X(20).
           05 MSG PIC X(50).
       COPY DGUESTBK.
           EXEC SQL
                INCLUDE SQLCA
           END-EXEC.
       COPY DFHAID.
       COPY DFHEIBLK. 
       PROCEDURE DIVISION.
    	    MOVE 'Enter title here' TO TITO.
    	    MOVE 'Enter message here' TO MSGO.
    	    EXEC CICS SEND MAP('GBKMAP') MAPSET('DGUESTBK')
             ERASE  
           END-EXEC 
           EXEC CICS RECEIVE MAP('GBKMAP') MAPSET('DGUESTBK') 
           END-EXEC 
           MOVE TITI TO TIT.
           MOVE MSGI TO MSG.
    	    EXEC SQL 
    		 INSERT INTO ENTRIES(TITLE,MESSAGE) VALUES(:TIT,:MSG)
    	    END-EXEC
    	    MOVE "GB01" TO TA.
    	    EXEC CICS RETURN
                TRANSID (TA)
           END-EXEC.
