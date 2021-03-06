#include "escrow.hpp"

namespace heymate {

ACTION escrow::create(
  uint64_t id,
  name client,
  name worker,
  uint64_t escrow,
  uint64_t reputation
) {
  require_auth(_self);

  eosio_assert(is_account(client.value), "client account does not exist");
  eosio_assert(is_account(worker.value), "worker account does not exist");

  eosio_assert(0 < escrow && escrow < 1000 , "escrow should be more than 0 and less than 1000");
  eosio_assert(0 < reputation && reputation < 1000, "reputation should be more than 0 and less than 1000");

  jobs_index jobs(_self, _self.value);
  auto found_job = jobs.find(id);

  eosio_assert(found_job == jobs.end(), "job with such id already exists");

  jobs.emplace(_self, [&](auto& job){
    job.id = id;
    job.client = client;
    job.worker = worker;
    job.escrow = escrow;
    job.reputation = reputation;
    job.success = false;
    job.complete = false;
  });

  //Call HMR burn for the worker
  eosio::action(
    permission_level{ _self, "active"_n },
    "reputation"_n, "burn"_n,
    std::make_tuple(worker, reputation) //name owner, uint64_t amount
  ).send();
}

ACTION escrow::release(uint64_t id, uint64_t reputation)
{
  require_auth(_self.value);

  jobs_index jobs(_self, _self.value);
  const auto& found_job = jobs.get(id, "no job object found");
  eosio_assert(!found_job.complete, "job is already completed");

  jobs.modify(found_job, _self, [&](auto& job){
    job.complete = true;
    job.success = true;
  });

  transfer_token(found_job.worker, found_job.escrow);
  mint_reputation(found_job.worker, reputation);
}

ACTION escrow::refund(uint64_t id, uint64_t cancellationLogic)
{
  require_auth(_self.value);

  jobs_index jobs(_self, _self.value);
  const auto& found_job = jobs.get(id, "no job object found");
  eosio_assert(!found_job.complete, "job is already completed");

  jobs.modify(found_job, _self, [&](auto& job){
    job.complete = true;
  });

  switch (cancellationLogic) {
    case 1: {
      transfer_token(found_job.client, found_job.escrow);
      break;
    }
    case 2: {
      mint_reputation(found_job.worker, found_job.reputation);
      break;
    }
    default: {
      transfer_token(found_job.client, found_job.escrow);
      mint_reputation(found_job.worker, found_job.reputation);
      break;
    }
  }

}

void escrow::transfer_token(name client, uint64_t escrow)
{
  //Call HEY transfer back to the client
  eosio::action(
    permission_level{ _self, "active"_n },
    "eosio.token"_n, "transfer"_n,
    std::make_tuple(_self.value, client, asset((escrow  * 10000) , symbol(symbol_code("HEY"), 4)), std::string(""))
  ).send();
}
void escrow::mint_reputation(name worker, uint64_t amount)
{
  //Call reputation mint for the worker
  eosio::action(
    permission_level{_self, "active"_n},
    "reputation"_n, "mint"_n,
    std::make_tuple(worker, amount)
  ).send();
}

} /// namespace heymate

EOSIO_DISPATCH(heymate::escrow, (create)(release)(refund))
