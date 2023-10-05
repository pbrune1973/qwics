package at.erste.uebung.arztpraxis;

import java.util.Date;

public class Arztpraxis {

    public static void main(String args[]) {
        Warteschlange w = new Warteschlange();
        Warteschlange2<Patient> patienten = new Warteschlange2<Patient>();

        Patient p = new Patient(4711,"Lieschen","Müller",new Date());
        w.fuegeAufgabeHinzu(p);
        patienten.fuegeAufgabeHinzu(p);

        w.fuegeAufgabeHinzu(new Anruf("+43 12334 566","Bittet um Rückruf"));

        p = new Patient(4712,"Erwin","Muster",new Date());
        w.fuegeAufgabeHinzu(p);
        patienten.fuegeAufgabeHinzu(p);

        p = new Patient(4713,"Micky","Maus",new Date());
        w.fuegeAufgabeHinzu(p);
        patienten.fuegeAufgabeHinzu(p);

        // Abarbeiten der Eingänge
        Aufgabe a = null;
        while ((a = w.naechsteAufgabe()) != null) {
            System.out.println(a.getBeschreibung());
        }

        System.out.println();

        // Abarbeiten der Patienten
        p = null;
        while ((p = patienten.naechsteAufgabe()) != null) {
            System.out.println(p.getBeschreibung());
        }
    }

}
