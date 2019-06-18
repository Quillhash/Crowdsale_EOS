import sys
from eosfactory.eosf import *
verbosity([Verbosity.INFO, Verbosity.OUT, Verbosity.DEBUG])

CONTRACT_WORKSPACE = sys.path[0] + "/../"
TOKEN_CONTRACT_WORKSPACE = sys.path[0] + "/../../quilltoken/"


def test():
    SCENARIO('''
    Execute all actions.
    ''')
    reset()
    create_master_account("eosio")

    #######################################################################################################
    
    COMMENT('''
    Create test accounts:
    ''')

    # test accounts and accounts where the smart contracts will be hosted
    create_account("alice", eosio, account_name="alice")
    create_account("crowdsaler", eosio, account_name="crowdsaler")
    create_account("eosiotoken", eosio, account_name="quilltoken")
    create_account("bob", eosio, account_name="bob")
    create_account("issuer", eosio, account_name="issuer")


    ########################################################################################################
    
    COMMENT('''
    Build and deploy token contract:
    ''')

    # creating token contract
    token_contract = Contract(eosiotoken, TOKEN_CONTRACT_WORKSPACE)
    token_contract.build(force=True)
    token_contract.deploy()

    ########################################################################################################
    
    COMMENT('''
    Build and deploy crowdsaler contract:
    ''')

    # creating crowdsaler contract
    contract = Contract(crowdsaler, CONTRACT_WORKSPACE)
    contract.build(force=True)
    contract.deploy()

    ########################################################################################################
    
    COMMENT('''
    Create EOS tokens 
    ''')

    token_contract.push_action(
        "create",
        {
            "issuer": eosio,
            "maximum_supply": "1000000000.0000 EOS"
        },
        [eosiotoken]
    )

    ########################################################################################################
    
    COMMENT('''
    Create QUI tokens 
    ''')

    token_contract.push_action(
        "create",
        {
            "issuer": crowdsaler,
            "maximum_supply": "1000000000.0000 QUI"
        },
        [eosiotoken]
    )

    ########################################################################################################

    COMMENT('''
    Issue EOS tokens to alice 
    ''')

    token_contract.push_action(
        "issue",
        {
            "to": alice,
            "quantity": "10000.0000 EOS",
            "memo": "issued tokens to alice"
        },
        [eosio]
    )

    COMMENT('''
    Issue EOS tokens to bob 
    ''')

    token_contract.push_action(
        "issue",
        {
            "to": bob,
            "quantity": "10000.0000 EOS",
            "memo": "issued tokens to bob"
        },
        [eosio]
    )

    ########################################################################################################

    COMMENT('''
    Issue QUI tokens to crowdsaler 
    ''')

    token_contract.push_action(
        "issue",
        {
            "to": crowdsaler,
            "quantity": "20000.0000 QUI",
            "memo": "issued tokens to alice"
        },
        [crowdsaler]
    )


    ########################################################################################################

    COMMENT('''
    Initialize the crowdsaler
    ''')

    contract.push_action(
        "init",
        {
            "recipient": issuer,
            "start": "2019-02-01T00:00:00",
            "finish": "2020-04-20T00:00:00"
        },
        [crowdsaler]
    )
    ########################################################################################################

    crowdsaler.push_action(
        "checkgoal",
         {},
        permission=(issuer, Permission.ACTIVE)
    )

    ########################################################################################################

    crowdsaler.push_action(
        "rate",
         {},
        permission=(issuer, Permission.ACTIVE)
    )

    ########################################################################################################

    COMMENT('''
    Invest in the crowdsaler 
    ''')

    # set eosio.code permission to the contract
    crowdsaler.set_account_permission(
        Permission.ACTIVE,
        {
            "threshold": 1,
            "keys": [
                {
                    "key": crowdsaler.active(),
                    "weight": 1
                }
            ],
            "accounts":
            [
                {
                    "permission":
                        {
                            "actor": crowdsaler,
                            "permission": "eosio.code"
                        },
                    "weight": 1
                }
            ]
        },
        Permission.OWNER,
        (crowdsaler, Permission.OWNER)
    )

    # transfer EOS tokens from alice to the host (contract) accounts
    eosiotoken.push_action(
        "transfer",
        {
            "from": alice,
            "to": crowdsaler,
            "quantity": "20.0000 EOS",
            "memo": "Invested 20 EOS in crowdsaler"
        },
        permission=(alice, Permission.ACTIVE)
    )

    # transfer EOS tokens from bob to the host (contract) accounts
    eosiotoken.push_action(
        "transfer",
        {
            "from": bob,
            "to": crowdsaler,
            "quantity": "30.0000 EOS",
            "memo": "Invested 30 EOS in crowdsaler"
        },
        permission=(bob, Permission.ACTIVE)
    )


    ########################################################################################################

    COMMENT('''
    Check table of the crowdsaler contract 
    ''')

    crowdsaler.table("deposit", crowdsaler)



    COMMENT('''
    Check tables of the quilltoken contract 
    ''')

    eosiotoken.table("stat", "QUI")
    eosiotoken.table("stat", "EOS")

    eosiotoken.table("accounts", alice)
    eosiotoken.table("accounts", bob)
    eosiotoken.table("accounts", crowdsaler)

    ########################################################################################################

    COMMENT('''
    withdraw EOS from the crowdsaler contract 
    ''')
    crowdsaler.push_action(
        "withdraw",
         {},
        permission=(issuer, Permission.ACTIVE)
    )

    ########################################################################################################

    COMMENT('''
    Check table of the crowdsaler and quilltoken contract after withdrawal
    ''')

    crowdsaler.table("deposit", crowdsaler)
    eosiotoken.table("accounts", crowdsaler)

    stop()

if __name__ == "__main__":
    test()
