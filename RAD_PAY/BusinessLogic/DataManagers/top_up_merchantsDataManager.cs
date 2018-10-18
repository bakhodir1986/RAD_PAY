using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class top_up_merchantsDataManager
    {
        //top_up_merchantsViewModel
//publicintid{get;set;}
//publicstringname{get;set;}
//publicint?status{get;set;}
//publicint?option{get;set;}
//publiclong?min_amount{get;set;}
//publiclong?max_amount{get;set;}
//publiclong?rate{get;set;}
//publicint?position{get;set;}
//publiclong?card_id{get;set;}
//publiclong?icon_id{get;set;}

        public static void Add(top_up_merchantsViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new top_up_merchants
            {
                id              = model.id          ,  
                name            = model.name        ,
                status          = model.status      ,
                option          = model.option      ,
                min_amount      = model.min_amount  ,
                max_amount      = model.max_amount  ,
                rate            = model.rate        ,
                position        = model.position    ,
                card_id         = model.card_id     ,
                icon_id         = model.icon_id     ,
            };

            db.top_up_merchants.Add(dbmodel);
        }

        public static void Modify(top_up_merchantsViewModel model, RAD_PAYEntities db)
        {
            var result = db.top_up_merchants.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id          ;  
                    dbmodel.name = model.name        ;
                    dbmodel.status = model.status      ;
                    dbmodel.option = model.option      ;
                    dbmodel.min_amount = model.min_amount  ;
                    dbmodel.max_amount = model.max_amount  ;
                    dbmodel.rate = model.rate        ;
                    dbmodel.position = model.position    ;
                    dbmodel.card_id = model.card_id     ;
                    dbmodel.icon_id = model.icon_id     ;
                }
            }
        }

        public static void Delete(top_up_merchantsViewModel model, RAD_PAYEntities db)
        {
            var result = db.top_up_merchants.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.top_up_merchants.Remove(dbmodel);
                }
            }
        }

        public static List<top_up_merchantsViewModel> Get(top_up_merchantsViewModel model, RAD_PAYEntities db)
        {
            List<top_up_merchantsViewModel> list = null;

            var query = from resmodel in db.top_up_merchants
                        select new top_up_merchantsViewModel
                        {
                            id = resmodel.id,
                            name = resmodel.name,
                            status = resmodel.status,
                            option = resmodel.option,
                            min_amount = resmodel.min_amount,
                            max_amount = resmodel.max_amount,
                            rate = resmodel.rate,
                            position = resmodel.position,
                            card_id = resmodel.card_id,
                            icon_id = resmodel.icon_id,
                        };

            list = query.ToList();

            return list;
        }
    }
}