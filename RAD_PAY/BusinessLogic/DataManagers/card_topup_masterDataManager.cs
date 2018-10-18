using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class card_topup_masterDataManager
    {
        //card_topup_masterViewModel
        //public long       card_id         { get; set; }
        //public string     number          { get; set; }
        //public string     expire          { get; set; }
        //public int?       status          { get; set; }
        //public string     name            { get; set; }
        //public string     pc_token        { get; set; }
        //public string     owner           { get; set; }
        //public string     password        { get; set; }
        //public long       balance         { get; set; }
        //public DateTime?  notify_ts       { get; set; }
        //public string     owner_phone     { get; set; }

        public static void Add(card_topup_masterViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new card_topup_master
            {
                card_id     = model.card_id     ,  
                number      = model.number      ,
                expire      = model.expire      ,
                status      = model.status      ,
                name        = model.name        ,
                pc_token    = model.pc_token    ,
                owner       = model.owner       ,
                password    = model.password    ,
                balance     = model.balance     ,
                notify_ts   = model.notify_ts   ,
                owner_phone = model.owner_phone ,
            };

            db.card_topup_master.Add(dbmodel);
        }

        public static void Modify(card_topup_masterViewModel model, RAD_PAYEntities db)
        {
            var result = db.card_topup_master.Where(z => z.card_id == model.card_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.card_id = model.card_id         ;  
                    dbmodel.number = model.number           ;
                    dbmodel.expire = model.expire           ;
                    dbmodel.status = model.status           ;
                    dbmodel.name = model.name               ;
                    dbmodel.pc_token = model.pc_token       ;
                    dbmodel.owner = model.owner             ;
                    dbmodel.password = model.password       ;
                    dbmodel.balance = model.balance         ;
                    dbmodel.notify_ts = model.notify_ts     ;
                    dbmodel.owner_phone = model.owner_phone;
                }
            }
        }

        public static void Delete(card_topup_masterViewModel model, RAD_PAYEntities db)
        {
            var result = db.card_topup_master.Where(z => z.card_id == model.card_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.card_topup_master.Remove(dbmodel);
                }
            }
        }

        public static List<card_topup_masterViewModel> Get(card_topup_masterViewModel model, RAD_PAYEntities db)
        {
            List<card_topup_masterViewModel> list = null;

            var query = from resmodel in db.card_topup_master
                        select new card_topup_masterViewModel
                        {
                            card_id         = resmodel.card_id,
                            number          = resmodel.number,
                            expire          = resmodel.expire,
                            status          = resmodel.status,
                            name            = resmodel.name,
                            pc_token        = resmodel.pc_token,
                            owner           = resmodel.owner,
                            password        = resmodel.password,
                            balance         = resmodel.balance,
                            notify_ts       = resmodel.notify_ts,
                            owner_phone     = resmodel.owner_phone,
                        };

            list = query.ToList();

            return list;
        }
    }
}