package at.erste.uebung;

import java.util.HashMap;

public class Worte {
    private static class WortInfo {
        public boolean ausgegeben;
        public int anzahl;

        public WortInfo(boolean ausgegeben, int anzahl) {
            this.ausgegeben = ausgegeben;
            this.anzahl = anzahl;
        }
    };

    public static void main(String args[]) {
        String satz = "Ich bin ein Text, der Text bin ich.";
        String[] worte = satz.split("[ .,]");
        HashMap<String,WortInfo> hauefigkeiten = new HashMap<String,WortInfo>();

        for (String wort : worte) {
            WortInfo h = new WortInfo(false,1);
            if (hauefigkeiten.containsKey(wort)) {
                h = hauefigkeiten.get(wort);
                h.anzahl++;
            }
            hauefigkeiten.put(wort,h);
        }

        for (String wort: worte) {
            WortInfo h = hauefigkeiten.get(wort);
            if (!h.ausgegeben) {
                System.out.println(wort+" " +h.anzahl);
                h.ausgegeben = true;
            }
        }
    }
}
