using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class card_bonusDataManager
    {
        //card_bonusViewModel
        //public long       card_id     { get; set; }
        //public string     number      { get; set; }
        //public string     expire      { get; set; }
        //public long?      xid         { get; set; }
        //public string     name        { get; set; }
        //public int?       tpl         { get; set; }
        //public string     pc_token    { get; set; }
        //public string     owner       { get; set; }
        //public string     password    { get; set; }
        //public long?      balance     { get; set; }
        //public DateTime?  notify_ts   { get; set; }
        //public string     owner_phone { get; set; }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="model"></param>
        /// <param name="db"></param>
        public static void Add(card_bonusViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new card_bonus
            {
                card_id     = model.card_id         ,   
                number      = model.number          ,
                expire      = model.expire          ,
                xid         = model.xid             ,
                name        = model.name            ,
                tpl         = model.tpl             ,
                pc_token    = model.pc_token        ,
                owner       = model.owner           ,
                password    = model.password        ,
                balance     = model.balance         ,
                notify_ts   = model.notify_ts       ,
                owner_phone = model.owner_phone     ,
            };

            db.card_bonus.Add(dbmodel);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="model"></param>
        /// <param name="db"></param>
        public static void Modify(card_bonusViewModel model, RAD_PAYEntities db)
        {
            var result = db.card_bonus.Where(z => z.card_id == model.card_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.card_id = model.card_id             ;   
                    dbmodel.number = model.number               ;
                    dbmodel.expire = model.expire               ;
                    dbmodel.xid = model.xid                     ;
                    dbmodel.name = model.name                   ;
                    dbmodel.tpl = model.tpl                     ;
                    dbmodel.pc_token = model.pc_token           ;
                    dbmodel.owner = model.owner                 ;
                    dbmodel.password = model.password           ;
                    dbmodel.balance = model.balance             ;
                    dbmodel.notify_ts = model.notify_ts         ;
                    dbmodel.owner_phone = model.owner_phone;
                }
            }
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="model"></param>
        /// <param name="db"></param>
        public static void Delete(card_bonusViewModel model, RAD_PAYEntities db)
        {
            var result = db.card_bonus.Where(z => z.card_id == model.card_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.card_bonus.Remove(dbmodel);
                }
            }
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="model"></param>
        /// <param name="db"></param>
        /// <returns></returns>
        public static List<card_bonusViewModel> Get(card_bonusViewModel model, RAD_PAYEntities db)
        {
            List<card_bonusViewModel> list = null;

            var query = from resmodel in db.card_bonus
                        select new card_bonusViewModel
                        {
                            card_id = resmodel.card_id,
                            number = resmodel.number,
                            expire = resmodel.expire,
                            xid = resmodel.xid,
                            name = resmodel.name,
                            tpl = resmodel.tpl,
                            pc_token = resmodel.pc_token,
                            owner = resmodel.owner,
                            password = resmodel.password,
                            balance = resmodel.balance,
                            notify_ts = resmodel.notify_ts,
                            owner_phone = resmodel.owner_phone,
                        };

            list = query.ToList();

            return list;
        }
    }
}