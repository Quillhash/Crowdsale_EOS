#include "crowdsaler.hpp"

#include "config.h"


// constructor
crowdsaler::crowdsaler(name self, name code, datastream<const char *> ds) : contract(self, code, ds),
                                                                            state_singleton(this->_self, this->_self.value), // code and scope both set to the contract's account
                                                                            reserved_singleton(this->_self, this->_self.value), // code and scope both set to the contract's account
                                                                            deposits(this->_self, this->_self.value),
                                                                            state(state_singleton.exists() ? state_singleton.get() : default_parameters()),
                                                                            reserved(reserved_singleton.exists() ? reserved_singleton.get() : default_parameters1())
                                                                            {}

// destructor
crowdsaler::~crowdsaler()
{
    this->state_singleton.set(this->state, this->_self); // persist the state of the crowdsale before destroying instance

    print("Saving state to the RAM ");
    print(this->state.toString());

    this->reserved_singleton.set(this->reserved, this->_self); // persist the state of the crowdsale before destroying instance

    print("Saving state to the RAM ");
    print(this->reserved.toString());    
}

// initialize the crowdfund
void crowdsaler::init(name recipient, time_point_sec start, time_point_sec finish)
{
    check(!this->state_singleton.exists(), "Already Initialzed");
    check(start < finish, "Start must be less than finish");
    require_auth(this->_self);
    check(recipient != this->_self,"Recipient should be different than contract deployer");
    // update state
    this->state.recipient = recipient;
    this->state.start = start;
    this->state.finish = finish;
    this->state.pause = false;
    
    asset goal = asset(GOAL, symbol("QUI", 4));
}

// issuance of only reserved QUI
void crowdsaler::issue(name to, asset quantity, uint64_t _class, std::string memo)
{
    require_auth(this->state.recipient);
    check( is_account( to ), "to account does not exist" );
    check( quantity.symbol == sy_qui ,"Can issue only QUI coins");
    switch(_class){
        case 1:
        check((this->reserved.class1.amount + quantity.amount) <= CLASS1MAX, "Cannot issue more than reserved quantity");
        this->reserved.class1 += quantity;
        break;
        case 2:
        check((this->reserved.class2.amount + quantity.amount) <= CLASS2MAX, "Cannot issue more than reserved quantity");
        this->reserved.class2 += quantity;
        break;        
        case 3:
        check((this->reserved.class3.amount + quantity.amount) <= CLASS3MAX, "Cannot issue more than reserved quantity");
        this->reserved.class3 += quantity;
        break;
        case 4:
        check((this->reserved.class4.amount + quantity.amount) <= CLASS4MAX, "Cannot issue more than reserved quantity");
        this->reserved.class4 += quantity;
        break;
        case 5:
        check((this->reserved.class5.amount + quantity.amount) <= CLASS5MAX, "Cannot issue more than reserved quantity");
        this->reserved.class5 += quantity;
        break;
        case 6:
        check((this->reserved.class6.amount + quantity.amount) <= CLASS6MAX, "Cannot issue more than reserved quantity");
        this->reserved.class6 += quantity;
        }

    handle_investment(to, quantity.amount);

    // issues QUI tokens to reserved class
    this->inline_issue(to, quantity, "Issue QUI tokens to reserved class successfull");

}

// update contract balances and send QUI tokens to the investor
void crowdsaler::handle_investment(name investor, uint64_t tokens_to_give)
{
    // hold the reference to the investor stored in the RAM
    auto it = this->deposits.find(investor.value);

    // if the depositor account was found, store his updated balances
    asset entire_tokens = asset(tokens_to_give, symbol("QUI", 4));
   
        if (it != this->deposits.end())
        {
            entire_tokens += it->tokens;
        }

        // if the depositor was not found create a new entry in the database, else update his balance
        if (it == this->deposits.end())
        {
            this->deposits.emplace(this->_self, [investor, entire_tokens](auto &deposit) {
                deposit.account = investor;
                deposit.tokens = entire_tokens;
            });
        }
        else
        {
            this->deposits.modify(it, this->_self, [investor, entire_tokens](auto &deposit) {
                deposit.account = investor;
                deposit.tokens = entire_tokens;
            });
        }
}



// handle transfers to this contract
void crowdsaler::buyquill(name from, name to, asset quantity, std::string memo)
{
    print(from);
    print(to);
    print(quantity);
    print(memo);
        
    if((to == this->_self)&&(quantity.symbol==sy_sys)){  

    check(this->state.pause==false, "Crowdsale has been paused" );
    
    // check timings of the eos crowdsale
    check(current_time_point().sec_since_epoch() >= this->state.start.utc_seconds, "Crowdsale hasn't started");

    // check the minimum and maximum contribution
    check(quantity.amount >= MIN_CONTRIB, "Contribution too low");
    check((quantity.amount <= MAX_CONTRIB) , "Contribution too high");

    // check if the Goal was reached
    check(this->state.total_tokens.amount <= GOAL, "GOAL reached");

    //calculate 3% fees on buying QUI
    asset fees ;
    fees.amount = quantity.amount*0.03;
    fees.symbol = sy_sys;
    
    this->state.total_eoses.amount += quantity.amount;

    quantity-=fees;
    // dont send fees to _self
    // else QUI supply would increase
    
    int64_t tokens_to_give; 
    if(current_time_point().sec_since_epoch() <= this->state.finish.utc_seconds){
        tokens_to_give = (quantity.amount)/RATE;
    }
    else tokens_to_give = (quantity.amount)/RATE2;

    // calculate from SYS to tokens
    this->state.total_tokens.amount += tokens_to_give; // calculate from SYS to tokens

    // handle investment
    this->handle_investment(from, tokens_to_give);

    // set the amounts to transfer, then call inline issue action to update balances in the token contract
    asset amount = asset(tokens_to_give, symbol("QUI", 4));

    this->inline_transfer(this->_self, from, amount, "Crowdsale deposit successful, bought QUI tokens"); 
    }
}

// used for trading.
void crowdsaler::transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo)
{
    require_auth(from);

    // calculate fees and send by rate...
    this->inline_transfer(from, to, quantity, "trading ");
  
}
// toggles unpause / pause contract
void crowdsaler::pause()
{
    require_auth(this->state.recipient);
    if (state.pause==false)
        this->state.pause = true; 
    else
        this->state.pause = false;
}

void crowdsaler::rate()
{
    print("Rate of 1 QUI token:");
    print(RATE);
}

void crowdsaler::checkgoal()
{
    if(this->state.total_tokens.amount <= GOAL)
    {
        print("Goal not Reached:");
        print(GOAL);
    }
    else
        print("Goal Reached");
}


// used by recipient to withdraw SYS
void crowdsaler::withdraw()
{
    require_auth(this->state.recipient);

    check(current_time_point().sec_since_epoch() >= this->state.finish.utc_seconds, "Crowdsale not ended yet" );
    check(this->state.total_eoses.amount >= SOFT_CAP_TKN, "Soft cap was not reached");
    eosio::asset all_eoses;
    all_eoses = this->state.total_eoses;
    this->inline_transfer(this->_self, this->state.recipient, all_eoses, "withdraw by recipient");
    //  crowdsaler deposit of eos deleted from table (which equals to total eoses in state)
    this->state.total_eoses.amount = 0;
    auto it = this->deposits.find(("crowdsaler"_n).value);
    this->deposits.erase(it);
}


// custom dispatcher that handles QUI transfers from quilltoken contract
extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action)
{
    if (code == eosio::name("quilltoken").value && action == eosio::name("transfer").value) // handle actions from eosio.token contract
    {
        eosio::execute_action(eosio::name(receiver), eosio::name(code), &crowdsaler::buyquill);
    }
    else if (code == receiver) // for other direct actions
    {
        switch (action)
        {
            EOSIO_DISPATCH_HELPER(crowdsaler, (init)(transfer)(issue)(pause)(rate)(checkgoal)(withdraw));
        }
    }
}
