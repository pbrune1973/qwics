package at.erste.uebung.arztpraxis;

import java.util.Date;

public class Patient implements Aufgabe {
    private long patientenId;
    private String vorname;
    private String nachname;
    private Date geburtsdatum;

    private Date eingang;

    public Patient(long patientenId, String vorname, String nachname, Date geburtsdatum) {
        this.patientenId = patientenId;
        this.vorname = vorname;
        this.nachname = nachname;
        this.geburtsdatum = geburtsdatum;
    }

    @Override
    public void setEingang(Date zeitpunkt) {
        this.eingang = zeitpunkt;
    }

    @Override
    public Date getEingang() {
        return this.eingang;
    }

    @Override
    public String getBeschreibung() {
        return "Patient "+patientenId+" "+nachname+", "+vorname+" ("+geburtsdatum+")";
    }
}
