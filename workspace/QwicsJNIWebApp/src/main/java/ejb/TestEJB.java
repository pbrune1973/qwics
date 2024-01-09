package ejb;

import javax.ejb.LocalBean;
import javax.ejb.Stateless;

@Stateless
@LocalBean
public class TestEJB {

    public String getLimit(String matchcode) {
        System.out.println("TestEJB called with "+matchcode);
        return "100000.00";
    }

}
