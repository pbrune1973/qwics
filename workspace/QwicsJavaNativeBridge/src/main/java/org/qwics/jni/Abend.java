package org.qwics.jni;

public class Abend extends Exception {
    public Abend() {
    }

    public Abend(String message) {
        super(message);
    }
}
