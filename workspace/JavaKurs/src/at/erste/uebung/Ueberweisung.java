package at.erste.uebung;

import java.util.Date;

public class Ueberweisung {
    private long auftragsNr;
    private Konto vonKto;
    private Konto zuKto;

    private double betrag;

    private Date valuta;
    private Date ausfuehrung;
    private String verwendungszweck;

    public Ueberweisung(long auftragsNr, Konto vonKto, Konto zuKto, double betrag, String verwendungszweck) {
        this.auftragsNr = auftragsNr;
        this.vonKto = vonKto;
        this.zuKto = zuKto;
        this.verwendungszweck = verwendungszweck;
    }

    public void ausfuehren() {
        vonKto.auszahlen(betrag);
        zuKto.einzahlen(betrag);
        this.valuta = new Date();
        this.ausfuehrung = new Date();
    }

}
