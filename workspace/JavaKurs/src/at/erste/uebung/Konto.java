package at.erste.uebung;

import at.erste.uebung.config.Bank;

public class Konto{
    private String iban;
    private String bicCode;
    private double saldo;

    private static String createIban(){
        return "AT000000";
    }

    public Konto(){
        this.saldo = 0;
        this.bicCode = Bank.UNSERE_BIC;
        this.iban = createIban();
    }

    public void einzahlen(double betrag) {
        this.saldo = this.saldo + betrag;
        System.out.println(this.saldo);
    }

    public void auszahlen(double betrag) {
		this.saldo -= betrag;
		System.out.println(this.saldo);
    }
}
