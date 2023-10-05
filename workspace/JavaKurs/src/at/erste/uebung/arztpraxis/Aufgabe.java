package at.erste.uebung.arztpraxis;

import java.util.Comparator;
import java.util.Date;

public interface Aufgabe {
    public void setEingang(Date zeitpunkt);
    public Date getEingang();

    public String getBeschreibung();
}
