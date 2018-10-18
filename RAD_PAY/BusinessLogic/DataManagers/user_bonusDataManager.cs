using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class user_bonusDataManager
    {
        //user_bonusViewModel
//publiclongid{get;set;}
//publiclonguid{get;set;}
//publiclong?balance{get;set;}
//publiclong?earns{get;set;}
//publicint?block{get;set;}
//publicstringfio{get;set;}
//publicstringpan{get;set;}
//publicstringexpire{get;set;}
//publicDateTime?add_ts{get;set;}
//publiclong?bonus_card_id{get;set;}

        public static void Add(user_bonusViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new user_bonus
            {
                    id              = model.id              ,      
                    uid             = model.uid             ,
                    balance         = model.balance         ,
                    earns           = model.earns           ,
                    block           = model.block           ,
                    fio             = model.fio             ,
                    pan             = model.pan             ,
                    expire          = model.expire          ,
                    add_ts          = model.add_ts          ,
                    bonus_card_id   = model.bonus_card_id   ,
            };

            db.user_bonus.Add(dbmodel);
        }

        public static void Modify(user_bonusViewModel model, RAD_PAYEntities db)
        {
            var result = db.user_bonus.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id              ;      
                    dbmodel.uid = model.uid             ;
                    dbmodel.balance = model.balance         ;
                    dbmodel.earns = model.earns           ;
                    dbmodel.block = model.block           ;
                    dbmodel.fio = model.fio             ;
                    dbmodel.pan = model.pan             ;
                    dbmodel.expire = model.expire          ;
                    dbmodel.add_ts = model.add_ts          ;
                    dbmodel.bonus_card_id = model.bonus_card_id   ;
                }
            }
        }

        public static void Delete(user_bonusViewModel model, RAD_PAYEntities db)
        {
            var result = db.user_bonus.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.user_bonus.Remove(dbmodel);
                }
            }
        }

        public static List<user_bonusViewModel> Get(user_bonusViewModel model, RAD_PAYEntities db)
        {
            List<user_bonusViewModel> list = null;

            var query = from resmodel in db.user_bonus
                        select new user_bonusViewModel
                        {
                            id = resmodel.id,
                            uid = resmodel.uid,
                            balance = resmodel.balance,
                            earns = resmodel.earns,
                            block = resmodel.block,
                            fio = resmodel.fio,
                            pan = resmodel.pan,
                            expire = resmodel.expire,
                            add_ts = resmodel.add_ts,
                            bonus_card_id = resmodel.bonus_card_id,
                        };

            list = query.ToList();

            return list;
        }
    }
}