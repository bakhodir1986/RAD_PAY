using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_bonusDataManager
    {
        //merchant_bonusViewModel
        //public int        merchant_id         { get; set; }
        //public long?      min_amount          { get; set; }
        //public int?       percent             { get; set; }
        //public string     description         { get; set; }
        //public DateTime?  start_date          { get; set; }
        //public DateTime?  end_date            { get; set; }
        //public long       id                  { get; set; }
        //public long?      bonus_amount        { get; set; }
        //public int?       status              { get; set; }
        //public double?    longitude           { get; set; }
        //public double?    latitude            { get; set; }
        //public int        group_id            { get; set; }

        public static void Add(merchant_bonusViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new merchant_bonus
            {
                merchant_id = model.merchant_id     ,
                min_amount  = model.min_amount      ,
                percent     = model.percent         ,
                description = model.description     ,
                start_date  = model.start_date      ,
                end_date    = model.end_date        ,
                id          = model.id              ,
                bonus_amount= model.bonus_amount    ,
                status      = model.status          ,
                longitude   = model.longitude       ,
                latitude    = model.latitude        ,
                group_id    = model.group_id        ,
            };

            db.merchant_bonus.Add(dbmodel);
        }

        public static void Modify(merchant_bonusViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_bonus.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.merchant_id = model.merchant_id     ;
                    dbmodel.min_amount = model.min_amount       ;
                    dbmodel.percent = model.percent             ;
                    dbmodel.description = model.description     ;
                    dbmodel.start_date = model.start_date       ;
                    dbmodel.end_date = model.end_date           ;
                    dbmodel.id = model.id                   ;
                    dbmodel.bonus_amount = model.bonus_amount    ;
                    dbmodel.status = model.status          ;
                    dbmodel.longitude = model.longitude       ;
                    dbmodel.latitude = model.latitude        ;
                    dbmodel.group_id = model.group_id        ;
                }
            }
        }

        public static void Delete(merchant_bonusViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_bonus.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.merchant_bonus.Remove(dbmodel);
                }
            }
        }

        public static List<merchant_bonusViewModel> Get(merchant_bonusViewModel model, RAD_PAYEntities db)
        {
            List<merchant_bonusViewModel> list = null;

            var query = from resmodel in db.merchant_bonus
                        select new merchant_bonusViewModel
                        {
                            merchant_id = resmodel.merchant_id,
                            min_amount = resmodel.min_amount,
                            percent = resmodel.percent,
                            description = resmodel.description,
                            start_date = resmodel.start_date,
                            end_date = resmodel.end_date,
                            id = resmodel.id,
                            bonus_amount = resmodel.bonus_amount,
                            status = resmodel.status,
                            longitude = resmodel.longitude,
                            latitude = resmodel.latitude,
                            group_id = resmodel.group_id,
                        };

            list = query.ToList();

            return list;
        }
    }
}