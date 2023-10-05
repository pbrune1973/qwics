package at.erste.uebung.arztpraxis;

import java.util.Date;

public class Anruf implements Aufgabe {
    private String nummer;
    private String betreff;
    private Date eingang;

    public Anruf(String nummer, String betreff) {
        this.nummer = nummer;
        this.betreff = betreff;
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
        return "Anruf "+nummer+" "+betreff+" "+eingang;
    }
}
