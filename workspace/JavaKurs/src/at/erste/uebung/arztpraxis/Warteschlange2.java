package at.erste.uebung.arztpraxis;

import java.util.ArrayList;
import java.util.Date;

public class Warteschlange2<T> {
    private ArrayList<T> aufgaben = new ArrayList<T>();

    public T naechsteAufgabe() {
        if (aufgaben.size() > 0) {
            T a = aufgaben.get(0);
            aufgaben.remove(0);
            return a;
        }
        return null;
    }

    public void fuegeAufgabeHinzu(T a) {
        aufgaben.add(a);
    }

}
