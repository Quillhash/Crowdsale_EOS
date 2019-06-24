# Crowdsale_EOS
Basic Crowdsale smart contract in EOS

#### Run the test cases
1. Locate both the files in your contract directory. 
2. Run ```python3 EOSCrowdsale/tests/test1.py```

#### Actions in Crowdsale smart contract 

To initialize the crowdsale with name of recipient (account which will manage the crowdsale, pause the campaign or withdraw the EOS collected ):

    

```ACTION init(eosio::name recipient, eosio::time_point_sec start, eosio::time_point_sec finish);```

To transfer the tokens from account “from” to the account “to”. It is redirected to “handle_investment” method where token transfer is managed & multi index table and states are managed.

 

```ACTION transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo);```

To withdraw all the EOS once the goal is achieved or the finish time set while initialising is passed by. It can be called by recipient only:

```ACTION withdraw()```

To avoid any discrepancies that may be present and for safety measure, crowdsale can be paused by the recipient at any moment of time. It acts as a toggle function so can be unpaused by calling the same action.



```ACTION pause(); ```

To get the rate of each token :

```ACTION rate(); ``` 



To check if goal is reached or not :  

 ```ACTION checkgoal();```

We will be issuing “QUI” token in exchange for “SYS” token of eosio.token contract after applying the rates. One can configure for the rates that apply after the crowdsale gets over.

