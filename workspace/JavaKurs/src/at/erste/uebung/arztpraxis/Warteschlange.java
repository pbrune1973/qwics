package at.erste.uebung.arztpraxis;

import java.sql.Array;
import java.util.ArrayList;
import java.util.Date;

public class Warteschlange {
    private ArrayList<Aufgabe> aufgaben = new ArrayList<Aufgabe>();

    public Aufgabe naechsteAufgabe() {
        if (aufgaben.size() > 0) {
            Aufgabe a = aufgaben.get(0);
            aufgaben.remove(0);
            return a;
        }
        return null;
    }

    public void fuegeAufgabeHinzu(Aufgabe a) {
        a.setEingang(new Date());
        aufgaben.add(a);
    }

}
